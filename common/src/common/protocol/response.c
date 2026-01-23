// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include "common/protocol.h"
#include "common/protocol/command.h"
#include "common/protocol/response.h"

#include "common.h"


#define PTR_U8(x) ((uint8_t *)(x))

void proto_res_init(ProtoRes *response, void *memory, uint16_t memorySize, uint8_t cmd) {
    response->cmd = cmd;

    switch (cmd) {
        case PROTO_CMD_GET_INFO:
            {
                ProtoResGetInfo *info = &response->response.getInfo;

                info->version.major = 0;
                info->version.minor = 0;

                info->packetSize = 0;
                info->features   = 0;
            }
            break;

        case PROTO_CMD_I2C_TRANSFER:
            {
                ProtoResI2cTransfer *t = &response->response.i2cTransfer;

                uint8_t overHead = 1;

                if (memorySize > overHead) {
                    t->rxBufferSize = memorySize - overHead;

                } else {
                    t->rxBufferSize = 0;
                }

                t->rxBuffer = NULL;
                t->status   = PROTO_I2C_STATUS_OK;
            }
            break;

        default:
            break;
    }
}

void proto_res_assign(ProtoRes *response, void *memory, uint16_t memorySize) {
    switch (response->cmd) {
        case PROTO_CMD_I2C_TRANSFER:
            {
                ProtoResI2cTransfer *t = &response->response.i2cTransfer;

                if (t->status == PROTO_I2C_STATUS_OK) {
                    if (t->rxBufferSize) {
                        t->rxBuffer = PTR_U8(memory) + 1;

                    } else {
                        t->rxBuffer = NULL;
                    }

                } else {
                    t->rxBuffer     = 0;
                    t->rxBufferSize = 0;
                }
            }
            break;

        default:
            break;
    }
}

uint16_t proto_res_encode(ProtoRes *response, void *memory, uint16_t memorySize) {
    uint16_t ret = 0;

    if (memory) {
        switch (response->cmd) {
            case PROTO_CMD_GET_INFO:
                {
                    ProtoResGetInfo *info = &response->response.getInfo;

                    PTR_U8(memory)[ret++] = (info->version.major << 4) | (info->version.minor & 0x0f);

                    ret += proto_int_val_encode(info->packetSize, PTR_U8(memory) + ret);

                    PTR_U8(memory)[ret++] = info->features;
                }
                break;

            case PROTO_CMD_I2C_TRANSFER:
                {
                    ProtoResI2cTransfer *t = &response->response.i2cTransfer;

                    PTR_U8(memory)[ret++] = t->status;

                    ret += t->rxBufferSize;
                }
                break;

            default:
                break;
        }
    }

    return ret;
}

bool proto_res_decode(ProtoRes *response, void *memory, uint16_t memorySize) {
    bool ret = memory != NULL && memorySize;

    if (ret) {
        uint8_t *memoryP = PTR_U8(memory);

        switch (response->cmd) {
            case PROTO_CMD_GET_INFO:
                {
                    ProtoResGetInfo *info = &response->response.getInfo;

                    if (ret) {
                        ret = memorySize != 0;
                        if (ret) {
                            info->version.major = (*memoryP) >> 4;
                            info->version.minor = (*memoryP) & 0x0f;

                            memoryP++;
                            memorySize--;
                        }
                    }

                    if (ret) {
                        ret = memorySize != 0;
                        if (ret) {
                            uint8_t lenSize = proto_int_val_length_probe(*memoryP);

                            ret = memorySize >= lenSize;
                            if (ret) {
                                info->packetSize = proto_int_val_decode(memoryP);

                                memoryP    += lenSize;
                                memorySize -= lenSize;
                            }
                        }
                    }

                    if (ret) {
                        ret = memorySize != 0;
                        if (ret) {
                            info->features = *memoryP;

                            memoryP++;
                            memorySize--;
                        }
                    }
                }
                break;

            case PROTO_CMD_I2C_TRANSFER:
                {
                    ProtoResI2cTransfer *t = &response->response.i2cTransfer;

                    t->rxBuffer     = NULL;
                    t->rxBufferSize = 0;

                    ret = memorySize != 0;
                    if (ret) {
                        t->status = *memoryP;

                        memoryP++;
                        memorySize--;
                    }

                    if (ret) {
                        if (t->status == PROTO_I2C_STATUS_OK) {
                            t->rxBufferSize = memorySize;
                            t->rxBuffer     = memoryP;
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
