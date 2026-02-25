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

#define DBG(x) spdlog::info x;
#define ERR(x) spdlog::error x;

#define CRC_POLY_OW 0x8C

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

static std::vector<std::unique_ptr<OwSlave>> _owSlaves;

static uint64_t _getOwRomCode(uint8_t familyCode, uint64_t sn) {
    uint64_t ret = (sn << 8) | familyCode;

    ret |= ((uint64_t) crc8_get((uint8_t *) &ret, 7, CRC_POLY_OW, 0) << 56);

    return ret;
}

static uint64_t _getOwRomCode(std::unique_ptr<OwSlave> &slave) {
    return _getOwRomCode(slave->getFamilyCode(), slave->getSerialNumber());
}

static void _ubusHubRequestCallback(ProtoReq *request, ProtoRes *response, void *callbackData) {
    auto *ctx = reinterpret_cast<Context *>(callbackData);

    DBG(("CALL"));

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
                            DBG(("PROTO_CMD_OW_TRANSFER [PROTO_OW_TRANSFER_TYPE_WRITE {}, data: {}]",
                                req.data.transfer.dataSize, spdlog::to_hex(
                                    req.data.transfer.data,
                                    req.data.transfer.data + req.data.transfer.dataSize
                                )
                            ));
                        }
                        break;

                    case PROTO_OW_TRANSFER_TYPE_READ:
                        {
                            DBG(("PROTO_CMD_OW_TRANSFER [PROTO_OW_TRANSFER_TYPE_READ {}]", req.data.transfer.dataSize));

                            if (res.data.transfer.dataSize == 9) {
                                res.data.transfer.data[0] = 0x00;
                                res.data.transfer.data[1] = 0xa2; // 10.125C
                                res.data.transfer.data[2] = 0x00;
                                res.data.transfer.data[3] = 0x00;
                                res.data.transfer.data[4] = 0x7f;
                                res.data.transfer.data[5] = 0xff;
                                res.data.transfer.data[6] = 0x00;
                                res.data.transfer.data[7] = 0x10;
                                res.data.transfer.data[8] = crc8_get(req.data.transfer.data, 8, 0x8c, 0);

                                res.data.transfer.dataSize = 9;
                            }
                        }
                        break;

                    case PROTO_OW_TRANSFER_TYPE_RESET:
                        {
                            DBG(("PROTO_CMD_OW_TRANSFER [PROTO_OW_TRANSFER_TYPE_RESET]"));

                            for (auto &slave : _owSlaves) {
                                slave->reset();
                            }

                            res.status = PROTO_OW_STATUS_OK;
                        }
                        break;

                    case PROTO_OW_TRANSFER_TYPE_SEARCH_START:
                        {
                            DBG(("PROTO_CMD_OW_TRANSFER [PROTO_OW_TRANSFER_TYPE_SEARCH_START]"));

                            res.status = PROTO_OW_STATUS_SEARCH_STEP;

                            res.data.search.descBit  = 0;
                            res.data.search.lastZero = 0;
                            res.data.search.romId    = _getOwRomCode(_owSlaves[0]);
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
                                owSearchLastZero++;
                                owSearchDescBit++;

                                owSearchRn = _getOwRomCode(_owSlaves[owSearchDescBit]);

                                if (owSearchLastZero == _owSlaves.size() - 1) {
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
    DBG(("START"));

    Context ctx;

    signal(SIGINT, _intHandler);

    {
        _owSlaves.emplace_back(std::make_unique<Ds1820>(true, 0x0000112233445500ULL));
        _owSlaves.emplace_back(std::make_unique<Ds1820>(true, 0x0000112233445501ULL));
        _owSlaves.emplace_back(std::make_unique<Ds1820>(true, 0x0000112233445502ULL));
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
