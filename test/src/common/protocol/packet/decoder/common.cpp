// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <gtest/gtest.h>

#include "common/protocol.h"
#include "common/protocol/command.h"
#include "common/protocol/packet/decoder.h"
#include "common/protocol/request.h"

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

TEST(common_protocol, decoder_misc) {
    ProtoPktDec decoder;
    ProtoPkt    packet;

    uint8_t buffer[32];

    proto_pkt_init(&packet, buffer, sizeof(buffer), 0, 0);

    // Wrong CRC
    {
        proto_pkt_dec_reset(&decoder, &packet);

        std::vector<uint8_t> data = genCmd(PROTO_CMD_GET_INFO, 1, {}, {});

        *data.rbegin() ^= 0x55;

        for (uint16_t i = 0; i < data.size() - 1; i++) {
            ASSERT_EQ(proto_pkt_dec_putByte(&decoder, data[i], &packet), PROTO_PKT_DES_RET_IDLE);
        }

        auto res = proto_pkt_dec_putByte(&decoder, *data.rbegin(), &packet);

        ASSERT_NE(res, PROTO_PKT_DES_RET_IDLE);
        ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(res), PROTO_ERROR_INVALID_CRC);
    }

    // Invalid length
    {
        proto_pkt_dec_reset(&decoder, &packet);

        std::vector<uint8_t> data = genCmd(
            PROTO_CMD_I2C_TRANSFER, 2, 
            [](ProtoReq &req) {
                req.request.i2cTransfer.dataSize = 128;
            },
            [](ProtoReq &req) {
            }
        );

        uint16_t i;
        for (i = 0; i < data.size(); i++) {
            auto res = proto_pkt_dec_putByte(&decoder, data[i], &packet);
            if (res != PROTO_PKT_DES_RET_IDLE) {
                ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(res), PROTO_ERROR_INVALID_LENGTH);
                break;
            }
        }

        ASSERT_NE(i, data.size());
    }
}