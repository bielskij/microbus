// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <gtest/gtest.h>

#include "common/protocol/command.h"
#include "common/protocol/request.h"

TEST(common_protocol, request_get_info) {
    uint8_t buffer[64];

    uint16_t bufferWritten;

    {
        ProtoReq request;

        proto_req_init(&request, buffer, sizeof(buffer), PROTO_CMD_GET_INFO);

        ASSERT_EQ(request.cmd, PROTO_CMD_GET_INFO);

        proto_req_assign(&request, buffer, sizeof(buffer));

        bufferWritten = proto_req_encode(&request, buffer, sizeof(buffer));

        ASSERT_EQ(bufferWritten, 0);
    }

    {
        ProtoReq decoded;

        proto_req_init(&decoded, nullptr, 0, PROTO_CMD_GET_INFO);
        proto_req_assign(&decoded, buffer, bufferWritten);

        ASSERT_TRUE(proto_req_decode(&decoded, buffer, bufferWritten));
    }
}

TEST(common_protocol, request_i2c_transfer) {
    uint8_t buffer[64];

    uint16_t bufferWritten;

    {
        ProtoReq request;

        proto_req_init(&request, buffer, sizeof(buffer), PROTO_CMD_I2C_TRANSFER);

        ASSERT_EQ(request.cmd, PROTO_CMD_I2C_TRANSFER);

        {
            ProtoReqI2CTransfer &t = request.request.i2cTransfer;

            ASSERT_EQ(t.data, nullptr);
            ASSERT_EQ(t.flags, 0);
            ASSERT_EQ(t.slaveAddress, 0);
            ASSERT_EQ(t.dataSize, sizeof(buffer) - 1);
        }
    }

    // flags check
    {
        ProtoReq request;

        proto_req_init(&request, buffer, sizeof(buffer), PROTO_CMD_I2C_TRANSFER);

        {
            ProtoReqI2CTransfer &t = request.request.i2cTransfer;

            {
                t.flags = \
                    PROTO_I2C_TRANSFER_FLAG_READ | 
                    PROTO_I2C_TRANSFER_FLAG_START;

                proto_req_assign(&request, buffer, sizeof(buffer));

                ASSERT_NE(t.dataSize, 0);

                t.dataSize = 0;

                ASSERT_EQ(proto_req_encode(&request, buffer, sizeof(buffer)), 3);

                t.dataSize = 128;

                bufferWritten = proto_req_encode(&request, buffer, sizeof(buffer));
                ASSERT_EQ(bufferWritten, 4);

                {
                    ProtoReq decoded;

                    proto_req_init(&decoded, nullptr, 0, request.cmd);

                    ASSERT_TRUE(proto_req_decode(&decoded, buffer, bufferWritten));

                    ASSERT_EQ(decoded.request.i2cTransfer.flags,        request.request.i2cTransfer.flags);
                    ASSERT_EQ(decoded.request.i2cTransfer.slaveAddress, request.request.i2cTransfer.slaveAddress);
                    ASSERT_EQ(decoded.request.i2cTransfer.data,         request.request.i2cTransfer.data);
                    ASSERT_EQ(decoded.request.i2cTransfer.dataSize,     request.request.i2cTransfer.dataSize);
                }
            }

            {
                t.flags = PROTO_I2C_TRANSFER_FLAG_START;

                proto_req_assign(&request, buffer, sizeof(buffer));

                ASSERT_EQ(t.dataSize, 128);

                ASSERT_EQ(proto_req_encode(&request, buffer, sizeof(buffer)), 130);

                t.dataSize = 10;

                bufferWritten = proto_req_encode(&request, buffer, sizeof(buffer));
                ASSERT_EQ(bufferWritten, 12);

                {
                    ProtoReq decoded;

                    proto_req_init(&decoded, nullptr, 0, request.cmd);

                    ASSERT_TRUE(proto_req_decode(&decoded, buffer, bufferWritten));

                    ASSERT_EQ(decoded.request.i2cTransfer.flags,        request.request.i2cTransfer.flags);
                    ASSERT_EQ(decoded.request.i2cTransfer.slaveAddress, request.request.i2cTransfer.slaveAddress);
                    ASSERT_EQ(decoded.request.i2cTransfer.data,         request.request.i2cTransfer.data);
                    ASSERT_EQ(decoded.request.i2cTransfer.dataSize,     request.request.i2cTransfer.dataSize);
                }
            }

            {
                t.flags = PROTO_I2C_TRANSFER_FLAG_REPEATED_START;

                proto_req_assign(&request, buffer, sizeof(buffer));

                ASSERT_EQ(t.dataSize, 10);

                ASSERT_EQ(proto_req_encode(&request, buffer, sizeof(buffer)), 12);

                t.dataSize = 10;

                bufferWritten = proto_req_encode(&request, buffer, sizeof(buffer));
                ASSERT_EQ(bufferWritten, 12);

                {
                    ProtoReq decoded;

                    proto_req_init(&decoded, nullptr, 0, request.cmd);

                    ASSERT_TRUE(proto_req_decode(&decoded, buffer, bufferWritten));

                    ASSERT_EQ(decoded.request.i2cTransfer.flags,        request.request.i2cTransfer.flags);
                    ASSERT_EQ(decoded.request.i2cTransfer.slaveAddress, request.request.i2cTransfer.slaveAddress);
                    ASSERT_EQ(decoded.request.i2cTransfer.data,         request.request.i2cTransfer.data);
                    ASSERT_EQ(decoded.request.i2cTransfer.dataSize,     request.request.i2cTransfer.dataSize);
                }
            }

            {
                t.flags = PROTO_I2C_TRANSFER_FLAG_STOP;

                proto_req_assign(&request, buffer, sizeof(buffer));

                ASSERT_EQ(t.dataSize, 10);

                ASSERT_EQ(proto_req_encode(&request, buffer, sizeof(buffer)), 11);

                t.dataSize = 10;

                bufferWritten = proto_req_encode(&request, buffer, sizeof(buffer));
                ASSERT_EQ(bufferWritten, 11);

                {
                    ProtoReq decoded;

                    proto_req_init(&decoded, nullptr, 0, request.cmd);

                    ASSERT_TRUE(proto_req_decode(&decoded, buffer, bufferWritten));

                    ASSERT_EQ(decoded.request.i2cTransfer.flags,        request.request.i2cTransfer.flags);
                    ASSERT_EQ(decoded.request.i2cTransfer.slaveAddress, request.request.i2cTransfer.slaveAddress);
                    ASSERT_EQ(decoded.request.i2cTransfer.data,         request.request.i2cTransfer.data);
                    ASSERT_EQ(decoded.request.i2cTransfer.dataSize,     request.request.i2cTransfer.dataSize);
                }
            }

            {
                t.flags = 0;

                proto_req_assign(&request, buffer, sizeof(buffer));

                ASSERT_EQ(t.dataSize, 10);

                ASSERT_EQ(proto_req_encode(&request, buffer, sizeof(buffer)), 11);

                t.dataSize = 10;

                bufferWritten = proto_req_encode(&request, buffer, sizeof(buffer));
                ASSERT_EQ(bufferWritten, 11);

                {
                    ProtoReq decoded;

                    proto_req_init(&decoded, nullptr, 0, request.cmd);

                    ASSERT_TRUE(proto_req_decode(&decoded, buffer, bufferWritten));

                    ASSERT_EQ(decoded.request.i2cTransfer.flags,        request.request.i2cTransfer.flags);
                    ASSERT_EQ(decoded.request.i2cTransfer.slaveAddress, request.request.i2cTransfer.slaveAddress);
                    ASSERT_EQ(decoded.request.i2cTransfer.data,         request.request.i2cTransfer.data);
                    ASSERT_EQ(decoded.request.i2cTransfer.dataSize,     request.request.i2cTransfer.dataSize);
                }
            }
        }
    }
}