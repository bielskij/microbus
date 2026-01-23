// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include "common/protocol.h"
#include "common/protocol/command.h"
#include "common/protocol/request.h"

#include "common.h"


#define PTR_U8(x) ((uint8_t *)(x))

void proto_req_init(ProtoReq *request, void *memory, uint16_t memorySize, uint8_t cmd) {
    request->cmd = cmd;

    switch (request->cmd) {
        case PROTO_CMD_I2C_TRANSFER:
            {
                ProtoReqI2CTransfer *t = &request->request.i2cTransfer;

                static const uint8_t overhead = 1; // flags

                if (memorySize > overhead) {
                    t->dataSize = memorySize - overhead;

                } else {
                    t->dataSize = 0;
                }

                t->data         = NULL;
                t->flags        = 0;
                t->slaveAddress = 0;
            }
            break;

        default:
            break;
    }
}

void proto_req_assign(ProtoReq *request, void *memory, uint16_t memorySize) {
    switch (request->cmd) {
        case PROTO_CMD_I2C_TRANSFER:
            {
                ProtoReqI2CTransfer *t = &request->request.i2cTransfer;

                uint8_t dataOffset = 1; // flags

                if (
                    t->flags & PROTO_I2C_TRANSFER_FLAG_START ||
                    t->flags & PROTO_I2C_TRANSFER_FLAG_REPEATED_START
                ) {
                    dataOffset++; // slave
                }

                if (t->flags & PROTO_I2C_TRANSFER_FLAG_READ) {
                    t->data = NULL;

                } else {
                    if (memory && memorySize >= dataOffset) {
                        t->data = PTR_U8(memory) + dataOffset;

                    } else {
                        t->data     = NULL;
                        t->dataSize = 0;
                    }
                }
            }
            break;

        default:
            break;
    }
}

uint16_t proto_req_encode(ProtoReq *request, void *memory, uint16_t memorySize) {
    uint16_t ret = 0;

    if (memory) {
        switch (request->cmd) {
            case PROTO_CMD_GET_INFO:
                {
                }
                break;

            case PROTO_CMD_I2C_TRANSFER:
                {
                    ProtoReqI2CTransfer *t = &request->request.i2cTransfer;

                    PTR_U8(memory)[ret++] = t->flags;

                    if (   
                        (t->flags & PROTO_I2C_TRANSFER_FLAG_START) ||
                        (t->flags & PROTO_I2C_TRANSFER_FLAG_REPEATED_START)
                    ) {
                        PTR_U8(memory)[ret++] = t->slaveAddress;
                    }

                    if (t->flags & PROTO_I2C_TRANSFER_FLAG_READ) {
                        ret += proto_int_val_encode(t->dataSize, PTR_U8(memory) + ret);

                    } else {
                        ret += t->dataSize;
                    }
                }
                break;

            default:
                {

                }
                break;
        }
    }

    return ret;
}

bool proto_req_decode(ProtoReq *request, void *memory, uint16_t memorySize) {
    bool ret = memory != NULL;

    if (ret) {
        uint8_t *memoryP = PTR_U8(memory);

        switch (request->cmd) {
            case PROTO_CMD_GET_INFO:
                break;

            case PROTO_CMD_I2C_TRANSFER:
                {
                    ProtoReqI2CTransfer *t = &request->request.i2cTransfer;

                    t->data     = NULL;
                    t->dataSize = 0;

                    if (ret) {
                        ret = memorySize != 0;
                        if (ret) {
                            t->flags = *memoryP;

                            memoryP++;
                            memorySize--;
                        }
                    }

                    if (ret) {
                        if (
                            t->flags & PROTO_I2C_TRANSFER_FLAG_START ||
                            t->flags & PROTO_I2C_TRANSFER_FLAG_REPEATED_START
                        ) {
                            ret = memorySize != 0;
                            if (ret) {
                                t->slaveAddress = *memoryP;

                                memoryP++;
                                memorySize--;
                            }
                        }
                    }

                    if (ret) {
                        if (t->flags & PROTO_I2C_TRANSFER_FLAG_READ) {
                            uint8_t lenSize = proto_int_val_length_probe(*memoryP);

                            ret = memorySize >= lenSize;
                            if (ret) {
                                t->dataSize = proto_int_val_decode(memoryP);

                                memoryP    += lenSize;
                                memorySize -= lenSize;
                            }

                        } else {
                            t->dataSize = memorySize;
                            t->data     = memoryP;
                        }
                    }
                }
                break;

            default:
                break;
        }
    }

    return ret;
}
