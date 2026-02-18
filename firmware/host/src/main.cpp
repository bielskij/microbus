// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#include "firmware/ubus.h"

#include "common/protocol/command.h"

#define DBG(x) spdlog::info x;
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

static const uint64_t ds1820Ids[] = {
    0x11000003a16db128ULL,
    0x1f000003a16a8728ULL,

    0x5CA14F02917C3A10ULL,
    0xE6031977B2148D10ULL,
    0x2B6D419E0AC25510ULL,
    0x975A88336E09F110ULL
};

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

                DBG(("PROTO_CMD_OW_TRANSFER"));

                switch (req.type) {
                    case PROTO_OW_TRANSFER_TYPE_RESET:
                        {
                            res.status = PROTO_OW_STATUS_OK;
                        }
                        break;

                    case PROTO_OW_TRANSFER_TYPE_SEARCH_START:
                        {
                            DBG(("PROTO_CMD_OW_TRANSFER [PROTO_OW_TRANSFER_TYPE_SEARCH_START]"));

                            res.status = PROTO_OW_STATUS_SEARCH_STEP;

                            res.data.search.descBit  = 0;
                            res.data.search.lastZero = 0;
                            res.data.search.romId    = ds1820Ids[0];
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

                            if (owSearchLastZero < 5) {
                                owSearchLastZero++;
                                owSearchDescBit++;

                                owSearchRn = ds1820Ids[owSearchDescBit];

                                res.status = PROTO_OW_STATUS_SEARCH_STEP;

                            } else {
                                owSearchDescBit  = 0;
                                owSearchLastZero = 0;

                                owSearchRn = ds1820Ids[5];

                                res.status = PROTO_OW_STATUS_SEARCH_DONE_FOUND;
                            }

                            res.data.search.descBit  = owSearchDescBit;
                            res.data.search.lastZero = owSearchLastZero;
                            res.data.search.romId    = owSearchRn;
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
