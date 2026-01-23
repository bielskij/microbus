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