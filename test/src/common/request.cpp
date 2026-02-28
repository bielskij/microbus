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

TEST(common_protocol, request_ow_transfer) {
    uint8_t buffer[64];

    uint16_t bufferWritten;

    {
        ProtoReq request;

        proto_req_init(&request, buffer, sizeof(buffer), PROTO_CMD_OW_TRANSFER);

        ASSERT_EQ(request.cmd, PROTO_CMD_OW_TRANSFER);

        {
            auto &t = request.request.owTransfer;

            ASSERT_EQ(t.type, PROTO_OW_TRANSFER_TYPE_UNKNOWN);
        }
    }

    {
        ProtoReq request;

        proto_req_init(&request, buffer, sizeof(buffer), PROTO_CMD_OW_TRANSFER);

        {
            auto &t = request.request.owTransfer;

            {
                t.type = PROTO_OW_TRANSFER_TYPE_WRITE;

                proto_req_assign(&request, buffer, sizeof(buffer));

                ASSERT_NE(t.data.transfer.dataSize, 0);

                t.data.transfer.dataSize = 0;

                ASSERT_EQ(proto_req_encode(&request, buffer, sizeof(buffer)), 1);

                t.data.transfer.dataSize = 128;

                bufferWritten = proto_req_encode(&request, buffer, sizeof(buffer));
                ASSERT_EQ(bufferWritten, 129);

                {
                    ProtoReq decoded;

                    decoded.request.owTransfer.type = request.request.owTransfer.type;

                    proto_req_init(&decoded, nullptr, 0, request.cmd);

                    ASSERT_TRUE(proto_req_decode(&decoded, buffer, bufferWritten));

                    ASSERT_EQ(decoded.request.owTransfer.type,                   request.request.owTransfer.type);
                    ASSERT_EQ(decoded.request.owTransfer.data.transfer.data,     request.request.owTransfer.data.transfer.data);
                    ASSERT_EQ(decoded.request.owTransfer.data.transfer.dataSize, request.request.owTransfer.data.transfer.dataSize);
                }
            }

            {
                t.type = PROTO_OW_TRANSFER_TYPE_READ;

                proto_req_assign(&request, buffer, sizeof(buffer));

                ASSERT_NE(t.data.transfer.dataSize, 0);
                ASSERT_EQ(t.data.transfer.data,     nullptr);

                request.request.owTransfer.data.transfer.dataSize = 1;

                ASSERT_EQ(proto_req_encode(&request, buffer, sizeof(buffer)), 2);

                request.request.owTransfer.data.transfer.dataSize = 190;

                ASSERT_EQ(proto_req_encode(&request, buffer, sizeof(buffer)), 3);

                {
                    ProtoReq decoded;

                    decoded.request.owTransfer.type = request.request.owTransfer.type;

                    proto_req_init(&decoded, nullptr, 0, request.cmd);

                    ASSERT_TRUE(proto_req_decode(&decoded, buffer, bufferWritten));

                    ASSERT_EQ(decoded.request.owTransfer.type,                   request.request.owTransfer.type);
                    ASSERT_EQ(decoded.request.owTransfer.data.transfer.data,     request.request.owTransfer.data.transfer.data);
                    ASSERT_EQ(decoded.request.owTransfer.data.transfer.dataSize, request.request.owTransfer.data.transfer.dataSize);
                }
            }

            {
                t.type = PROTO_OW_TRANSFER_TYPE_RESET;

                proto_req_assign(&request, buffer, sizeof(buffer));

                ASSERT_EQ(proto_req_encode(&request, buffer, sizeof(buffer)), 1);

                {
                    ProtoReq decoded;

                    decoded.request.owTransfer.type = request.request.owTransfer.type;

                    proto_req_init(&decoded, nullptr, 0, request.cmd);

                    ASSERT_TRUE(proto_req_decode(&decoded, buffer, bufferWritten));

                    ASSERT_EQ(decoded.request.owTransfer.type, request.request.owTransfer.type);
                }
            }

            {
                t.type = PROTO_OW_TRANSFER_TYPE_SEARCH_START;

                proto_req_assign(&request, buffer, sizeof(buffer));

                t.data.search.type = 0xf0;

                ASSERT_EQ(proto_req_encode(&request, buffer, sizeof(buffer)), 2);

                {
                    ProtoReq decoded;

                    decoded.request.owTransfer.type = request.request.owTransfer.type;

                    proto_req_init(&decoded, nullptr, 0, request.cmd);

                    ASSERT_TRUE(proto_req_decode(&decoded, buffer, bufferWritten));

                    ASSERT_EQ(decoded.request.owTransfer.type,             request.request.owTransfer.type);
                    ASSERT_EQ(decoded.request.owTransfer.data.search.type, request.request.owTransfer.data.search.type);
                }
            }

            {
                t.type = PROTO_OW_TRANSFER_TYPE_SEARCH_STEP;

                proto_req_assign(&request, buffer, sizeof(buffer));

                request.request.owTransfer.data.search.romId     = 0x1122334455667788ULL;
                request.request.owTransfer.data.search.lastZero  = 1;
                request.request.owTransfer.data.search.romId     = 2;
                request.request.owTransfer.data.search.type      = 0xf0;

                ASSERT_EQ(proto_req_encode(&request, buffer, sizeof(buffer)), 12);

                {
                    ProtoReq decoded;

                    decoded.request.owTransfer.type = request.request.owTransfer.type;

                    proto_req_init(&decoded, nullptr, 0, request.cmd);

                    ASSERT_TRUE(proto_req_decode(&decoded, buffer, bufferWritten));

                    ASSERT_EQ(decoded.request.owTransfer.type,                  request.request.owTransfer.type);
                    ASSERT_EQ(decoded.request.owTransfer.data.search.romId,     request.request.owTransfer.data.search.romId);
                    ASSERT_EQ(decoded.request.owTransfer.data.search.lastZero,  request.request.owTransfer.data.search.lastZero);
                    ASSERT_EQ(decoded.request.owTransfer.data.search.descBit,   request.request.owTransfer.data.search.descBit);
                    ASSERT_EQ(decoded.request.owTransfer.data.search.type,      request.request.owTransfer.data.search.type);
                }
            }

            {
                t.type = PROTO_OW_TRANSFER_TYPE_TOUCH_BIT;

                proto_req_assign(&request, buffer, sizeof(buffer));

                request.request.owTransfer.data.touchBit.value = 1;

                ASSERT_EQ(proto_req_encode(&request, buffer, sizeof(buffer)), 2);

                {
                    ProtoReq decoded;

                    decoded.request.owTransfer.type = request.request.owTransfer.type;

                    proto_req_init(&decoded, nullptr, 0, request.cmd);

                    ASSERT_TRUE(proto_req_decode(&decoded, buffer, bufferWritten));

                    ASSERT_EQ(decoded.request.owTransfer.type,                request.request.owTransfer.type);
                    ASSERT_EQ(decoded.request.owTransfer.data.touchBit.value, request.request.owTransfer.data.touchBit.value);
                }
            }
        }
    }
}