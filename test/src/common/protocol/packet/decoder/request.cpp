// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <vector>
#include <functional>

#include <gtest/gtest.h>

#include "common/protocol.h"
#include "common/protocol/command.h"
#include "common/protocol/packet/decoder.h"
#include "common/protocol/request.h"

struct RequestDecoderTestData {
    uint8_t                            cmd;
    std::function<void(ProtoReq &req)> prepareReq;
    std::function<void(ProtoReq &req)> fillReq;
    std::function<void(ProtoReq &req)> validateReq;
};

class RequestDecoderTestWithParameter : public testing::TestWithParam<RequestDecoderTestData> {
};

static std::vector<uint8_t> genCmd(uint8_t cmd, uint8_t id, std::function<void(ProtoReq &req)> prepareReq, std::function<void(ProtoReq &req)> fillReq) {
    std::vector<uint8_t> ret;

    uint8_t  packetBuffer[512];
    ProtoPkt packet;

    proto_pkt_init(&packet, packetBuffer, sizeof(packetBuffer), cmd, id);

    {
        ProtoReq request;

        proto_req_init(&request, packet.payload, packet.payloadSize, packet.code);

        if (prepareReq) {
            prepareReq(request);
        }

        proto_req_assign(&request, packet.payload, packet.payloadSize);

        if (fillReq) {
            fillReq(request);
        }

        packet.payloadUsed = proto_req_encode(&request, packet.payload, packet.payloadSize);
    }

    if (proto_pkt_encode(&packet)) {
        ret.insert(ret.end(), packet.header,  packet.header  + packet.headerUsed);
        ret.insert(ret.end(), packet.payload, packet.payload + packet.payloadUsed);
        ret.insert(ret.end(), packet.footer,  packet.footer  + packet.footerUsed);
    }

    return ret;
}

TEST_P(RequestDecoderTestWithParameter, common_protocol) {
    auto data = genCmd(
        GetParam().cmd, 1,
        [](ProtoReq &res) {
            auto &f = GetParam().prepareReq;
            if (f) {
                f(res);
            }
        },
        [](ProtoReq &res) {
            auto &f = GetParam().fillReq;
            if (f) {
                f(res);
            }
        }
    );

    ASSERT_FALSE(data.empty());

    {
        ProtoPktDec decoder;
        ProtoPkt    packet;
        uint16_t    packetDataSize = data.size();
        uint8_t     packetData[packetDataSize];

        proto_pkt_init(&packet, packetData, packetDataSize, 0, 0);

        proto_pkt_dec_reset(&decoder, &packet);

        for (auto i = 0; i < data.size() - 1; i++) {
            ASSERT_EQ(proto_pkt_dec_putByte(&decoder, data[i], &packet), PROTO_PKT_DES_RET_IDLE);
        }

        auto res = proto_pkt_dec_putByte(&decoder, *data.rbegin(), &packet);

        ASSERT_NE(res, PROTO_PKT_DES_RET_IDLE);

        ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(res), PROTO_NO_ERROR);

        ASSERT_EQ(packet.code, GetParam().cmd);
        ASSERT_EQ(packet.id,   1);

        {
            ProtoReq request;

            proto_req_init(&request, packet.payload, packet.payloadUsed, packet.code);

            ASSERT_TRUE(proto_req_decode(&request, packet.payload, packet.payloadUsed));

            if (GetParam().validateReq) {
                GetParam().validateReq(request);
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(common_protocol, RequestDecoderTestWithParameter, testing::Values(
    // Get info
    RequestDecoderTestData {
        PROTO_CMD_GET_INFO,
        [](ProtoReq &req){
        },
        [](ProtoReq &req){
        },
        [](ProtoReq &req){
        },
    },

    // Start, read 64
    RequestDecoderTestData {
        PROTO_CMD_I2C_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            t.flags        = PROTO_I2C_TRANSFER_FLAG_START | PROTO_I2C_TRANSFER_FLAG_READ;
            t.slaveAddress = 0x34;
            t.dataSize     = 64;
        },
        [](ProtoReq &req){
        },
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            ASSERT_EQ(t.dataSize,     64);
            ASSERT_EQ(t.slaveAddress, 0x34);
            ASSERT_EQ(t.flags,        PROTO_I2C_TRANSFER_FLAG_START | PROTO_I2C_TRANSFER_FLAG_READ);
            ASSERT_EQ(t.data,         nullptr);
        },
    },

    // Start, read 256
    RequestDecoderTestData {
        PROTO_CMD_I2C_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            t.flags        = PROTO_I2C_TRANSFER_FLAG_START | PROTO_I2C_TRANSFER_FLAG_READ;
            t.slaveAddress = 0x34;
            t.dataSize     = 256;
        },
        [](ProtoReq &req){
        },
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            ASSERT_EQ(t.dataSize,     256);
            ASSERT_EQ(t.slaveAddress, 0x34);
            ASSERT_EQ(t.flags,        PROTO_I2C_TRANSFER_FLAG_START | PROTO_I2C_TRANSFER_FLAG_READ);
            ASSERT_EQ(t.data,         nullptr);
        },
    },

    // Start, write 64
    RequestDecoderTestData {
        PROTO_CMD_I2C_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            t.flags        = PROTO_I2C_TRANSFER_FLAG_START;
            t.slaveAddress = 0x34;
            t.dataSize     = 64;
        },
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            for (uint16_t i = 0; i < t.dataSize; i++) {
                t.data[i] = i;
            }
        },
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            ASSERT_EQ(t.dataSize,     64);
            ASSERT_EQ(t.slaveAddress, 0x34);
            ASSERT_EQ(t.flags,        PROTO_I2C_TRANSFER_FLAG_START);
            ASSERT_NE(t.data,         nullptr);

            for (uint16_t i = 0; i < t.dataSize; i++) {
                ASSERT_EQ(t.data[i], i);
            }
        },
    },

    // Start, write 256
    RequestDecoderTestData {
        PROTO_CMD_I2C_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            t.flags        = PROTO_I2C_TRANSFER_FLAG_START;
            t.slaveAddress = 0x34;
            t.dataSize     = 256;
        },
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            for (uint16_t i = 0; i < t.dataSize; i++) {
                t.data[i] = i;
            }
        },
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            ASSERT_EQ(t.dataSize,     256);
            ASSERT_EQ(t.slaveAddress, 0x34);
            ASSERT_EQ(t.flags,        PROTO_I2C_TRANSFER_FLAG_START);
            ASSERT_NE(t.data,         nullptr);

            for (uint16_t i = 0; i < t.dataSize; i++) {
                ASSERT_EQ(t.data[i], i);
            }
        },
    },

    // Read 64
    RequestDecoderTestData {
        PROTO_CMD_I2C_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            t.flags        = PROTO_I2C_TRANSFER_FLAG_READ;
            t.slaveAddress = 0x34;
            t.dataSize     = 64;
        },
        [](ProtoReq &req){
        },
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            ASSERT_EQ(t.dataSize,     64);
            ASSERT_EQ(t.slaveAddress, 0);
            ASSERT_EQ(t.flags,        PROTO_I2C_TRANSFER_FLAG_READ);
            ASSERT_EQ(t.data,         nullptr);
        },
    },

    // Read 256
    RequestDecoderTestData {
        PROTO_CMD_I2C_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            t.flags        = PROTO_I2C_TRANSFER_FLAG_READ;
            t.slaveAddress = 0x34;
            t.dataSize     = 256;
        },
        [](ProtoReq &req){
        },
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            ASSERT_EQ(t.dataSize,     256);
            ASSERT_EQ(t.slaveAddress, 0);
            ASSERT_EQ(t.flags,        PROTO_I2C_TRANSFER_FLAG_READ);
            ASSERT_EQ(t.data,         nullptr);
        },
    },

    // Write 64
    RequestDecoderTestData {
        PROTO_CMD_I2C_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            t.flags        = 0;
            t.slaveAddress = 0x34;
            t.dataSize     = 64;
        },
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            for (uint16_t i = 0; i < t.dataSize; i++) {
                t.data[i] = i;
            }
        },
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            ASSERT_EQ(t.dataSize,     64);
            ASSERT_EQ(t.slaveAddress, 0);
            ASSERT_EQ(t.flags,        0);
            ASSERT_NE(t.data,         nullptr);

            for (uint16_t i = 0; i < t.dataSize; i++) {
                ASSERT_EQ(t.data[i], i);
            }
        },
    },

    // Write 256
    RequestDecoderTestData {
        PROTO_CMD_I2C_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            t.flags        = 0;
            t.slaveAddress = 0x34;
            t.dataSize     = 256;
        },
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            for (uint16_t i = 0; i < t.dataSize; i++) {
                t.data[i] = i;
            }
        },
        [](ProtoReq &req){
            auto &t = req.request.i2cTransfer;

            ASSERT_EQ(t.dataSize,     256);
            ASSERT_EQ(t.slaveAddress, 0);
            ASSERT_EQ(t.flags,        0);
            ASSERT_NE(t.data,         nullptr);

            for (uint16_t i = 0; i < t.dataSize; i++) {
                ASSERT_EQ(t.data[i], i);
            }
        },
    },

    // OW reset
    RequestDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            t.type = PROTO_OW_TRANSFER_TYPE_RESET;
        },
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;
        },
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            ASSERT_EQ(t.type, PROTO_OW_TRANSFER_TYPE_RESET);
        }
    },

    RequestDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            t.type = PROTO_OW_TRANSFER_TYPE_SEARCH_START;
        },
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;
        },
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            ASSERT_EQ(t.type, PROTO_OW_TRANSFER_TYPE_SEARCH_START);
        }
    },

    RequestDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            t.type = PROTO_OW_TRANSFER_TYPE_SEARCH_STEP;

            t.data.search.romId     = 0x8877665544332211ULL;
            t.data.search.descBit   = 1;
            t.data.search.lastZero  = 2;
        },
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;
        },
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            ASSERT_EQ(t.type, PROTO_OW_TRANSFER_TYPE_SEARCH_STEP);

            ASSERT_EQ(t.data.search.romId,     0x8877665544332211ULL);
            ASSERT_EQ(t.data.search.descBit,   1);
            ASSERT_EQ(t.data.search.lastZero,  2);
        }
    },

    RequestDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            t.type = PROTO_OW_TRANSFER_TYPE_READ;
        },
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            t.data.transfer.dataSize = 256;
        },
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            ASSERT_EQ(t.type, PROTO_OW_TRANSFER_TYPE_READ);

            ASSERT_EQ(t.data.transfer.dataSize, 256);
        }
    },

    RequestDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            t.type = PROTO_OW_TRANSFER_TYPE_WRITE;
        },
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            t.data.transfer.dataSize = 223;

            for (uint8_t i = 0; i < t.data.transfer.dataSize; i++) {
                t.data.transfer.data[i] = i;
            }

        },
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            ASSERT_EQ(t.type, PROTO_OW_TRANSFER_TYPE_WRITE);

            ASSERT_EQ(t.data.transfer.dataSize, 223);

            for (uint8_t i = 0; i < t.data.transfer.dataSize; i++) {
                ASSERT_EQ(t.data.transfer.data[i], i);
            }
        }
    },

    RequestDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            t.type = PROTO_OW_TRANSFER_TYPE_TOUCH_BIT;
        },
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            t.data.touchBit.value = 1;
        },
        [](ProtoReq &req){
            auto &t = req.request.owTransfer;

            ASSERT_EQ(t.type, PROTO_OW_TRANSFER_TYPE_TOUCH_BIT);

            ASSERT_EQ(t.data.touchBit.value, 1);
        }
    },

    RequestDecoderTestData {
        PROTO_CMD_SPI_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.spiTransfer;

            t.flags = PROTO_SPI_TRANSFER_FLAG_KEEP_CS;
            t.rxBufferSize = 256;
            t.rxSkipSize   = 256;
            t.txBufferSize = 256;
        },
        [](ProtoReq &req){
            auto &t = req.request.spiTransfer;

            for (int i = 0; i < t.txBufferSize; i++) {
                t.txBuffer[i] = i;
            }
        },
        [](ProtoReq &req){
            auto &t = req.request.spiTransfer;

            ASSERT_EQ(t.flags, PROTO_SPI_TRANSFER_FLAG_KEEP_CS);

            ASSERT_EQ(t.rxBufferSize, 256);
            ASSERT_EQ(t.txBufferSize, 256);
            ASSERT_EQ(t.rxSkipSize,   256);

            for (int i = 0; i < t.txBufferSize; i++) {
                ASSERT_EQ(t.txBuffer[i], i);
            }
        }
    },

    RequestDecoderTestData {
        PROTO_CMD_SPI_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.spiTransfer;

            t.flags = PROTO_SPI_TRANSFER_FLAG_KEEP_CS;
            t.rxBufferSize = 16;
            t.rxSkipSize   = 16;
            t.txBufferSize = 16;
        },
        [](ProtoReq &req){
            auto &t = req.request.spiTransfer;

            for (int i = 0; i < t.txBufferSize; i++) {
                t.txBuffer[i] = i;
            }
        },
        [](ProtoReq &req){
            auto &t = req.request.spiTransfer;

            ASSERT_EQ(t.flags, PROTO_SPI_TRANSFER_FLAG_KEEP_CS);

            ASSERT_EQ(t.rxBufferSize, 16);
            ASSERT_EQ(t.txBufferSize, 16);
            ASSERT_EQ(t.rxSkipSize,   16);

            for (int i = 0; i < t.txBufferSize; i++) {
                ASSERT_EQ(t.txBuffer[i], i);
            }
        }
    },

    RequestDecoderTestData {
        PROTO_CMD_SPI_TRANSFER,
        [](ProtoReq &req){
            auto &t = req.request.spiTransfer;

            t.flags = 0;
            t.rxBufferSize = 0;
            t.rxSkipSize   = 0;
            t.txBufferSize = 0;
        },
        [](ProtoReq &req){
            auto &t = req.request.spiTransfer;
        },
        [](ProtoReq &req){
            auto &t = req.request.spiTransfer;

            ASSERT_EQ(t.flags, 0);

            ASSERT_EQ(t.rxBufferSize, 0);
            ASSERT_EQ(t.txBufferSize, 0);
            ASSERT_EQ(t.rxSkipSize,   0);
        }
    }

    // RequestDecoderTestData {
    //     PROTO_CMD_I2C_TRANSFER,
    //     [](ProtoReq &req){
    //     },
    //     [](ProtoReq &req){
    //     },
    //     [](ProtoReq &req){
    //     },
    // }
));