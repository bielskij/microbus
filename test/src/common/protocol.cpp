// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <gtest/gtest.h>

#include "common/protocol.h"
#include "common/protocol/packet.h"

#define BYTE_PATTERN 0xa5

TEST(common_protocol, packet_init) {
    ProtoPkt packet;

    // Empty
    {
        proto_pkt_init(&packet, nullptr, 0, 3, 64);

        ASSERT_EQ(packet.id,   64);
        ASSERT_EQ(packet.code, 3);

        ASSERT_EQ(packet.header,     nullptr);
        ASSERT_EQ(packet.headerSize, 0);
        ASSERT_EQ(packet.headerUsed, 0);

        ASSERT_EQ(packet.payload,     nullptr);
        ASSERT_EQ(packet.payloadSize, 0);
        ASSERT_EQ(packet.payloadUsed, 0);
    }

    // small
    {
        uint8_t buffer[64];

        proto_pkt_init(&packet, buffer, sizeof(buffer), 3, 64);

        ASSERT_EQ(packet.id,   64);
        ASSERT_EQ(packet.code, 3);

        ASSERT_EQ(packet.header,     buffer);
        ASSERT_EQ(packet.headerSize, 3);
        ASSERT_EQ(packet.headerUsed, 0);

        ASSERT_EQ(packet.payload,     buffer + packet.headerSize);
        ASSERT_EQ(packet.payloadSize, sizeof(buffer) - packet.headerSize - 1);
        ASSERT_EQ(packet.payloadUsed, 0);
    }

    // large
    {
        uint8_t buffer[256];

        proto_pkt_init(&packet, buffer, sizeof(buffer), 3, 64);

        ASSERT_EQ(packet.id,   64);
        ASSERT_EQ(packet.code, 3);

        ASSERT_EQ(packet.header,     buffer);
        ASSERT_EQ(packet.headerSize, 4);
        ASSERT_EQ(packet.headerUsed, 0);

        ASSERT_EQ(packet.payload,     buffer + packet.headerSize);
        ASSERT_EQ(packet.payloadSize, sizeof(buffer) - packet.headerSize - 1);
        ASSERT_EQ(packet.payloadUsed, 0);
    }

    // border case
    {
        uint8_t buffer[132];

        proto_pkt_init(&packet, buffer, sizeof(buffer) + 1, 3, 64);
        ASSERT_EQ(packet.headerSize,    4);
        ASSERT_EQ(packet.payloadSize, 128);
        ASSERT_EQ(packet.header,      buffer);
        ASSERT_EQ(packet.payload,     buffer + packet.headerSize);
        ASSERT_EQ(proto_pkt_size(&packet), sizeof(buffer) + 1);

        proto_pkt_init(&packet, buffer, sizeof(buffer), 3, 64);
        ASSERT_EQ(packet.headerSize,    3);
        ASSERT_EQ(packet.payloadSize, 127);
        ASSERT_EQ(packet.header,      buffer);
        ASSERT_EQ(packet.payload,     buffer + packet.headerSize);
        ASSERT_EQ(proto_pkt_size(&packet), sizeof(buffer) - 1);

        proto_pkt_init(&packet, buffer, sizeof(buffer) - 1, 3, 64);
        ASSERT_EQ(packet.headerSize,    3);
        ASSERT_EQ(packet.payloadSize, 127);
        ASSERT_EQ(packet.header,      buffer);
        ASSERT_EQ(packet.payload,     buffer + packet.headerSize);
        ASSERT_EQ(proto_pkt_size(&packet), sizeof(buffer) - 1);

        proto_pkt_init(&packet, buffer, sizeof(buffer) - 2, 3, 64);
        ASSERT_EQ(packet.headerSize,    3);
        ASSERT_EQ(packet.payloadSize, 126);
        ASSERT_EQ(packet.header,      buffer);
        ASSERT_EQ(packet.payload,     buffer + packet.headerSize);
        ASSERT_EQ(proto_pkt_size(&packet), sizeof(buffer) - 2);
    }
}

TEST(common_protocol, packet_cleanup) {
    ProtoPkt packet;

    uint8_t buffer[64];

    {
        proto_pkt_init(&packet, buffer, sizeof(buffer), 1, 128);

        packet.headerUsed  = packet.headerSize;
        packet.payloadUsed = packet.payloadSize;

        ASSERT_EQ(packet.headerUsed,   3);
        ASSERT_EQ(packet.payloadUsed, 60);
    }

    {
        proto_pkt_clear(&packet);

        ASSERT_EQ(packet.headerUsed,   0);
        ASSERT_EQ(packet.headerSize,   3);
        ASSERT_EQ(packet.payloadUsed,  0);
        ASSERT_EQ(packet.payloadSize, 60);

        ASSERT_EQ(packet.id,   128);
        ASSERT_EQ(packet.code, 1);

        ASSERT_EQ(packet.header,  buffer);
        ASSERT_EQ(packet.payload, buffer + packet.headerSize);
    }
}

TEST(common_protocol, packet_encode) {
    ProtoPkt packet;

    uint8_t buffer[64];
    uint8_t code = 3;
    uint8_t id   = 12;

    memset(buffer, BYTE_PATTERN, sizeof(buffer));

    {
        proto_pkt_init(&packet, buffer, sizeof(buffer), code, id);

        packet.payloadUsed = packet.payloadSize;

        ASSERT_TRUE(proto_pkt_encode(&packet));

        ASSERT_EQ(packet.header[0], PROTO_SYNC_NIBBLE | code);
        ASSERT_EQ(packet.header[1], id);
        ASSERT_EQ(packet.header[2], packet.payloadSize);
        ASSERT_EQ(packet.header[3], 0xa5);

        ASSERT_EQ(packet.footer[0], 0x7c);
    }

    {
        proto_pkt_init(&packet, buffer, sizeof(buffer), code, id);

        packet.payloadUsed = packet.payloadSize + 1;

        ASSERT_FALSE(proto_pkt_encode(&packet));

        ASSERT_EQ(packet.footer[0],  0);
        ASSERT_EQ(packet.headerUsed, 0);
        ASSERT_EQ(packet.headerSize, 3);
    }
}

TEST(common_protocol, packet_size) {
    ProtoPkt packet;

    uint8_t buffer[64];

    proto_pkt_init(&packet, buffer, sizeof(buffer), 0, 0);

    ASSERT_EQ(proto_pkt_size(&packet), sizeof(buffer));
}