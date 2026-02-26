// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#include "firmware/ubus.h"

#include "ow/slave.h"
#include "ow/slave/ds1820.h"

#include "common/crc8.h"
#include "common/protocol/command.h"

#define DBG(x) spdlog::debug x;
#define ERR(x) spdlog::error x;

struct Context {
    int         ptyMasterFd;
    std::string ptyMasterPath;

    UbusHub hub;

    Context() {
        this->ptyMasterFd = -1;
    }
};

static const uint16_t packetSize = 512;
static uint8_t        packetBuffer[packetSize];

static volatile bool interrupted = false;

static std::vector<std::shared_ptr<OwSlave>> _owSlaves;
static std::shared_ptr<OwSlave>              _owSlave;

static uint64_t _getOwRomCode(uint8_t familyCode, uint64_t sn) {
    uint64_t ret = (sn << 8) | familyCode;

    ret |= ((uint64_t) crc8_get((uint8_t *) &ret, 7, CRC_POLY_OW, 0) << 56);

    return ret;
}

static uint64_t _getOwRomCode(std::shared_ptr<OwSlave> &slave) {
    return _getOwRomCode(slave->getFamilyCode(), slave->getSerialNumber());
}

static uint64_t _getOwRomCode(const uint8_t data[8]) {
    uint64_t ret;

    ret  = data[7]; ret <<= 8;
    ret |= data[6]; ret <<= 8;
    ret |= data[5]; ret <<= 8;
    ret |= data[4]; ret <<= 8;
    ret |= data[3]; ret <<= 8;
    ret |= data[2]; ret <<= 8;
    ret |= data[1]; ret <<= 8;
    ret |= data[0];

    return ret;
}

static void _ubusHubRequestCallback(ProtoReq *request, ProtoRes *response, void *callbackData) {
    auto *ctx = reinterpret_cast<Context *>(callbackData);

    switch (request->cmd) {
        case PROTO_CMD_GET_INFO:
            {
                DBG(("PROTO_CMD_GET_INFO"));

                response->response.getInfo.features = PROTO_FEATURE_I2C | PROTO_FEATURE_OW;
            }
            break;

        case PROTO_CMD_I2C_TRANSFER:
            {
                auto &t = request->request.i2cTransfer;

                DBG(("PROTO_CMD_I2C_TRANSFER {}-{:x} | {}: {} | {}",
                    t.flags & PROTO_I2C_TRANSFER_FLAG_START ? "STA" :
                        t.flags & PROTO_I2C_TRANSFER_FLAG_REPEATED_START ? "RSTA" : "-",
                    t.flags & (PROTO_I2C_TRANSFER_FLAG_START | PROTO_I2C_TRANSFER_FLAG_REPEATED_START) ? t.slaveAddress : 0,
                    t.flags & PROTO_I2C_TRANSFER_FLAG_READ ? 'R' : 'W',
                    t.dataSize,
                    t.flags & PROTO_I2C_TRANSFER_FLAG_STOP ? "STO": "-"
                ));

                if (! (t.flags & PROTO_I2C_TRANSFER_FLAG_READ)) {
                    spdlog::info("Data to write {:a16}", spdlog::to_hex(t.data, t.data + t.dataSize));
                }
            }
            break;

        case PROTO_CMD_OW_TRANSFER:
            {
                auto &req = request->request.owTransfer;
                auto &res = response->response.owTransfer;

                switch (req.type) {
                    case PROTO_OW_TRANSFER_TYPE_WRITE:
                        {
                            DBG(("PROTO_CMD_OW_TRANSFER [PROTO_OW_TRANSFER_TYPE_WRITE {}]", req.data.transfer.dataSize));

                            if (_owSlave) {
                                _owSlave->write(req.data.transfer.data, req.data.transfer.dataSize);

                            } else {
                                auto *dataPtr  = req.data.transfer.data;
                                auto  dataSize = req.data.transfer.dataSize;

                                if (dataSize > 0) {
                                    // Match ROM
                                    if (dataPtr[0] == 0x55) {
                                        auto expectedRomId = _getOwRomCode(dataPtr + 1);

                                        DBG(("Received Match ROM command with ROM code {:x}", expectedRomId));

                                        for (auto &slave : _owSlaves) {
                                            if (_getOwRomCode(slave) == expectedRomId) {
                                                _owSlave = slave;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        break;

                    case PROTO_OW_TRANSFER_TYPE_READ:
                        {
                            DBG(("PROTO_CMD_OW_TRANSFER [PROTO_OW_TRANSFER_TYPE_READ {}]", req.data.transfer.dataSize));

                            res.data.transfer.dataSize = req.data.transfer.dataSize;

                            if (_owSlave) {
                                _owSlave->read(res.data.transfer.data, res.data.transfer.dataSize);

                            } else {
                                memset(res.data.transfer.data, 0, res.data.transfer.dataSize);
                            }
                        }
                        break;

                    case PROTO_OW_TRANSFER_TYPE_RESET:
                        {
                            DBG(("PROTO_CMD_OW_TRANSFER [PROTO_OW_TRANSFER_TYPE_RESET]"));

                            for (auto &slave : _owSlaves) {
                                slave->reset();
                            }

                            _owSlave.reset();

                            res.status = PROTO_OW_STATUS_OK;
                        }
                        break;

                    case PROTO_OW_TRANSFER_TYPE_SEARCH_START:
                        {
                            DBG(("PROTO_CMD_OW_TRANSFER [PROTO_OW_TRANSFER_TYPE_SEARCH_START]"));

                            if (_owSlaves.empty()) {
                                res.status = PROTO_OW_STATUS_SEARCH_DONE_EMPTY;

                            } else {
                                res.data.search.descBit  = 1;
                                res.data.search.lastZero = 1;
                                res.data.search.romId    = _getOwRomCode(_owSlaves[0]);

                                if (_owSlaves.size() > 1) {
                                    res.status = PROTO_OW_STATUS_SEARCH_STEP;

                                } else {
                                    res.status = PROTO_OW_STATUS_SEARCH_DONE_FOUND;
                                }
                            }
                        }
                        break;

                    case PROTO_OW_TRANSFER_TYPE_SEARCH_STEP:
                        {
                            DBG(("PROTO_CMD_OW_TRANSFER [PROTO_OW_TRANSFER_TYPE_SEARCH_STEP: {}, {}, {:x}]",
                                req.data.search.descBit, req.data.search.lastZero, req.data.search.romId
                            ));

                            auto owSearchDescBit  = req.data.search.descBit;
                            auto owSearchLastZero = req.data.search.lastZero;
                            auto owSearchRn       = req.data.search.romId;

                            if (owSearchLastZero < _owSlaves.size()) {
                                owSearchRn = _getOwRomCode(_owSlaves[owSearchDescBit]);

                                owSearchLastZero++;
                                owSearchDescBit++;

                                if (owSearchDescBit == _owSlaves.size()) {
                                    res.status = PROTO_OW_STATUS_SEARCH_DONE_FOUND;

                                } else {
                                    res.status = PROTO_OW_STATUS_SEARCH_STEP;
                                }

                                res.data.search.descBit  = owSearchDescBit;
                                res.data.search.lastZero = owSearchLastZero;
                                res.data.search.romId    = owSearchRn;

                            } else {
                                res.status = PROTO_OW_STATUS_SEARCH_DONE_EMPTY;
                            }
                        }
                        break;

                    case PROTO_OW_TRANSFER_TYPE_TOUCH_BIT:
                        {
                            DBG(("PROTO_CMD_OW_TRANSFER [PROTO_OW_TRANSFER_TYPE_TOUCH_BIT: {}]", req.data.touchBit.value));

                            res.data.touchBit.value = 1;
                        }
                        break;
                }
            }
            break;

        default:
            {
                DBG(("Unknown command %d", request->cmd));
            }
            break;
    }
}

static void _ubusHubResponseCallback(uint8_t *buffer, uint16_t bufferSize, void *callbackData) {
    auto *ctx = reinterpret_cast<Context *>(callbackData);

    // spdlog::info("response:\n{:a16}", spdlog::to_hex(buffer, buffer + bufferSize));

    write(ctx->ptyMasterFd, buffer, bufferSize);
}

static void _intHandler(int signo) {
    DBG(("Received signal {}, exiting", signo));

    interrupted = true;
}

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::debug);

    DBG(("START"));

    Context ctx;

    signal(SIGINT, _intHandler);

    {
        _owSlaves.emplace_back(std::make_shared<Ds1820>(true, 0x0000112233445500ULL, -10.876));
        _owSlaves.emplace_back(std::make_shared<Ds1820>(true, 0x0000112233445501ULL, 22.1234));
        _owSlaves.emplace_back(std::make_shared<Ds1820>(true, 0x0000112233445502ULL, 12.1256));
    }

    do {
        ctx.ptyMasterFd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (ctx.ptyMasterFd < 0) {
            ERR(("Unable to open PTY!"));

            break;
        }

        {
            ctx.ptyMasterPath.resize(FILENAME_MAX);

            if (ptsname_r(ctx.ptyMasterFd, ctx.ptyMasterPath.data(), FILENAME_MAX) != 0) {
                ERR(("Unable to get PTY path!"));

                break;
            }

            DBG(("New PTY created at '{}'", ctx.ptyMasterPath));
        }

        if (grantpt(ctx.ptyMasterFd) != 0) {
            ERR(("Unable to grant access to the slave pseudoterminal!"));

            break;
        }

        if (unlockpt(ctx.ptyMasterFd) != 0) {
            ERR(("Unable to unlock master/slave pair!"));

            break;
        }

        ubus_hub_setup(&ctx.hub, packetBuffer, packetSize, _ubusHubRequestCallback, _ubusHubResponseCallback, &ctx);

        while (! interrupted) {
            fd_set readSet;

            struct timeval timeout = {
                .tv_sec  = 0,
                .tv_usec = 500 * 1000
            };

            FD_ZERO(&readSet);
            FD_SET(ctx.ptyMasterFd, &readSet);

            int selectRet = ::select(ctx.ptyMasterFd + 1, &readSet, NULL, NULL, &timeout);
            if (selectRet < 0) {
                if (errno != EINTR) {
                    ERR(("Select returned an error: {}", std::strerror(errno)));
                }

            } else if (selectRet > 0) {
                if (FD_ISSET(ctx.ptyMasterFd, &readSet)) {
                    uint8_t byte;

                    if (::read(ctx.ptyMasterFd, &byte, 1) == 1) {
                        ubus_hub_putByte(&ctx.hub, byte);
                    }

                } else {
                    ubus_hub_reset(&ctx.hub);
                }
            }
        }

        DBG(("Main loop terminated"));
    } while (0);

    if (ctx.ptyMasterFd >= 0) {
        close(ctx.ptyMasterFd);
    }

    return 0;
}
