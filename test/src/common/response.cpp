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

TEST(common_protocol, response_ow_transfer) {
    uint8_t buffer[64];

    uint16_t bufferWritten;

    {
        ProtoRes response;

        {
            proto_res_init(&response, buffer, sizeof(buffer), PROTO_CMD_OW_TRANSFER);

            ASSERT_EQ(response.cmd,                      PROTO_CMD_OW_TRANSFER);
            ASSERT_EQ(response.response.owTransfer.type, PROTO_OW_TRANSFER_TYPE_UNKNOWN);

            response.response.owTransfer.type   = PROTO_OW_TRANSFER_TYPE_RESET;
            response.response.owTransfer.status = PROTO_OW_STATUS_OK;

            proto_res_assign(&response, buffer, sizeof(buffer));

            bufferWritten = proto_res_encode(&response, buffer, sizeof(buffer));
            ASSERT_EQ(bufferWritten, 1);

            {
                ProtoRes decoded;

                proto_res_init(&decoded, nullptr, 0, response.cmd);

                decoded.response.owTransfer.type = response.response.owTransfer.type;

                proto_res_assign(&decoded, buffer, bufferWritten);

                ASSERT_TRUE(proto_res_decode(&decoded, buffer, bufferWritten));

                ASSERT_EQ(decoded.response.owTransfer.status, response.response.owTransfer.status);
                ASSERT_EQ(decoded.response.owTransfer.type,   response.response.owTransfer.type);
            }
        }

        {
            proto_res_init(&response, buffer, sizeof(buffer), PROTO_CMD_OW_TRANSFER);

            response.response.owTransfer.type   = PROTO_OW_TRANSFER_TYPE_READ;
            response.response.owTransfer.status = PROTO_OW_STATUS_OK;

            proto_res_assign(&response, buffer, sizeof(buffer));

            ASSERT_NE(response.response.owTransfer.data.transfer.data,     nullptr);
            ASSERT_NE(response.response.owTransfer.data.transfer.dataSize, 0);

            response.response.owTransfer.data.transfer.dataSize = 32;

            for (uint8_t i = 0; i < response.response.owTransfer.data.transfer.dataSize; i++) {
                response.response.owTransfer.data.transfer.data[i] = i;
            }

            bufferWritten = proto_res_encode(&response, buffer, sizeof(buffer));
            ASSERT_EQ(bufferWritten, 33);

            {
                ProtoRes decoded;

                proto_res_init(&decoded, nullptr, 0, response.cmd);

                decoded.response.owTransfer.type = response.response.owTransfer.type;

                proto_res_assign(&decoded, buffer, bufferWritten);

                ASSERT_TRUE(proto_res_decode(&decoded, buffer, bufferWritten));

                ASSERT_EQ(decoded.response.owTransfer.status, response.response.owTransfer.status);
                ASSERT_EQ(decoded.response.owTransfer.type,   response.response.owTransfer.type);

                ASSERT_EQ(decoded.response.owTransfer.data.transfer.data,     response.response.owTransfer.data.transfer.data);
                ASSERT_EQ(decoded.response.owTransfer.data.transfer.dataSize, response.response.owTransfer.data.transfer.dataSize);

                for (int i = 0; i < decoded.response.owTransfer.data.transfer.dataSize; i++) {
                    ASSERT_EQ(decoded.response.owTransfer.data.transfer.data[i], i);
                }
            }
        }

        {
            proto_res_init(&response, buffer, sizeof(buffer), PROTO_CMD_OW_TRANSFER);

            response.response.owTransfer.type   = PROTO_OW_TRANSFER_TYPE_WRITE;
            response.response.owTransfer.status = PROTO_OW_STATUS_OK;

            proto_res_assign(&response, buffer, sizeof(buffer));

            bufferWritten = proto_res_encode(&response, buffer, sizeof(buffer));
            ASSERT_EQ(bufferWritten, 1);

            {
                ProtoRes decoded;

                proto_res_init(&decoded, nullptr, 0, response.cmd);

                decoded.response.owTransfer.type = response.response.owTransfer.type;

                proto_res_assign(&decoded, buffer, bufferWritten);

                ASSERT_TRUE(proto_res_decode(&decoded, buffer, bufferWritten));

                ASSERT_EQ(decoded.response.owTransfer.status, response.response.owTransfer.status);
                ASSERT_EQ(decoded.response.owTransfer.type,   response.response.owTransfer.type);
            }
        }

        {
            proto_res_init(&response, buffer, sizeof(buffer), PROTO_CMD_OW_TRANSFER);

            response.response.owTransfer.type   = PROTO_OW_TRANSFER_TYPE_SEARCH_START;
            response.response.owTransfer.status = PROTO_OW_STATUS_SEARCH_END;

            proto_res_assign(&response, buffer, sizeof(buffer));

            bufferWritten = proto_res_encode(&response, buffer, sizeof(buffer));
            ASSERT_EQ(bufferWritten, 1);

            {
                ProtoRes decoded;

                proto_res_init(&decoded, nullptr, 0, response.cmd);

                decoded.response.owTransfer.type = response.response.owTransfer.type;

                proto_res_assign(&decoded, buffer, bufferWritten);

                ASSERT_TRUE(proto_res_decode(&decoded, buffer, bufferWritten));

                ASSERT_EQ(decoded.response.owTransfer.status, response.response.owTransfer.status);
                ASSERT_EQ(decoded.response.owTransfer.type,   response.response.owTransfer.type);
            }
        }

        {
            proto_res_init(&response, buffer, sizeof(buffer), PROTO_CMD_OW_TRANSFER);

            response.response.owTransfer.type   = PROTO_OW_TRANSFER_TYPE_SEARCH_STEP;
            response.response.owTransfer.status = PROTO_OW_STATUS_SEARCH_STEP;

            proto_res_assign(&response, buffer, sizeof(buffer));

            response.response.owTransfer.data.searchStep.romId     = 0x1122334455667788ULL;
            response.response.owTransfer.data.searchStep.descBit   = 1;
            response.response.owTransfer.data.searchStep.lastZero  = 2;
            response.response.owTransfer.data.searchStep.searchBit = 3;

            bufferWritten = proto_res_encode(&response, buffer, sizeof(buffer));
            ASSERT_EQ(bufferWritten, 12);

            {
                ProtoRes decoded;

                proto_res_init(&decoded, nullptr, 0, response.cmd);

                decoded.response.owTransfer.type = response.response.owTransfer.type;

                proto_res_assign(&decoded, buffer, bufferWritten);

                ASSERT_TRUE(proto_res_decode(&decoded, buffer, bufferWritten));

                ASSERT_EQ(decoded.response.owTransfer.status, response.response.owTransfer.status);
                ASSERT_EQ(decoded.response.owTransfer.type,   response.response.owTransfer.type);

                ASSERT_EQ(decoded.response.owTransfer.data.searchStep.descBit,   response.response.owTransfer.data.searchStep.descBit);
                ASSERT_EQ(decoded.response.owTransfer.data.searchStep.lastZero,  response.response.owTransfer.data.searchStep.lastZero);
                ASSERT_EQ(decoded.response.owTransfer.data.searchStep.romId,     response.response.owTransfer.data.searchStep.romId);
                ASSERT_EQ(decoded.response.owTransfer.data.searchStep.searchBit, response.response.owTransfer.data.searchStep.searchBit);
            }
        }
    }
}