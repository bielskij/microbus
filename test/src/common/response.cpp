// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <gtest/gtest.h>

#include "common/protocol/command.h"
#include "common/protocol/response.h"

TEST(common_protocol, response_get_info) {
    uint8_t buffer[64];

    uint16_t bufferWritten;

    {
        ProtoRes response;

        {
            proto_res_init(&response, buffer, sizeof(buffer), PROTO_CMD_GET_INFO);

            ASSERT_EQ(response.cmd, PROTO_CMD_GET_INFO);

            proto_res_assign(&response, buffer, sizeof(buffer));

            response.response.getInfo.features      = PROTO_FEATURE_I2C;
            response.response.getInfo.packetSize    = 16 * 1024;
            response.response.getInfo.version.major = 10;
            response.response.getInfo.version.minor = 12;

            bufferWritten = proto_res_encode(&response, buffer, sizeof(buffer));

            ASSERT_EQ(bufferWritten, 4);
        }

        {
            ProtoRes decoded;

            proto_res_init(&decoded, nullptr, 0, response.cmd);
            proto_res_assign(&decoded, buffer, bufferWritten);

            ASSERT_TRUE(proto_res_decode(&decoded, buffer, bufferWritten));

            ASSERT_EQ(decoded.response.getInfo.features,      response.response.getInfo.features);
            ASSERT_EQ(decoded.response.getInfo.packetSize,    response.response.getInfo.packetSize);
            ASSERT_EQ(decoded.response.getInfo.version.major, response.response.getInfo.version.major);
            ASSERT_EQ(decoded.response.getInfo.version.minor, response.response.getInfo.version.minor);
        }
    }
}

TEST(common_protocol, response_i2c_transfer) {
    uint8_t buffer[64];

    uint16_t bufferWritten;

    {
        ProtoRes response;

        {
            proto_res_init(&response, buffer, sizeof(buffer), PROTO_CMD_I2C_TRANSFER);

            ASSERT_EQ(response.cmd, PROTO_CMD_I2C_TRANSFER);

            response.response.i2cTransfer.status       = PROTO_I2C_STATUS_OK;
            response.response.i2cTransfer.rxBufferSize = 12;

            proto_res_assign(&response, buffer, sizeof(buffer));

            bufferWritten = proto_res_encode(&response, buffer, sizeof(buffer));

            ASSERT_EQ(bufferWritten, 13);
        }

        {
            ProtoRes decoded;

            proto_res_init(&decoded, nullptr, 0, response.cmd);
            proto_res_assign(&decoded, buffer, bufferWritten);

            ASSERT_TRUE(proto_res_decode(&decoded, buffer, bufferWritten));

            ASSERT_EQ(decoded.response.i2cTransfer.status,       response.response.i2cTransfer.status);
            ASSERT_EQ(decoded.response.i2cTransfer.rxBuffer,     response.response.i2cTransfer.rxBuffer);
            ASSERT_EQ(decoded.response.i2cTransfer.rxBufferSize, response.response.i2cTransfer.rxBufferSize);
        }
    }

    {
        ProtoRes response;

        {
            proto_res_init(&response, buffer, sizeof(buffer), PROTO_CMD_I2C_TRANSFER);

            ASSERT_EQ(response.cmd, PROTO_CMD_I2C_TRANSFER);

            response.response.i2cTransfer.status       = PROTO_I2C_STATUS_NAK_ADDRESS;
            response.response.i2cTransfer.rxBufferSize = 12;

            proto_res_assign(&response, buffer, sizeof(buffer));

            bufferWritten = proto_res_encode(&response, buffer, sizeof(buffer));

            ASSERT_EQ(bufferWritten, 1);
        }

        {
            ProtoRes decoded;

            proto_res_init(&decoded, nullptr, 0, response.cmd);
            proto_res_assign(&decoded, buffer, bufferWritten);

            ASSERT_TRUE(proto_res_decode(&decoded, buffer, bufferWritten));

            ASSERT_EQ(decoded.response.i2cTransfer.status,       response.response.i2cTransfer.status);
            ASSERT_EQ(decoded.response.i2cTransfer.rxBuffer,     nullptr);
            ASSERT_EQ(decoded.response.i2cTransfer.rxBufferSize, 0);
        }
    }
}