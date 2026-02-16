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

        case PROTO_CMD_OW_TRANSFER:
            {
                ProtoResOwTransfer *t = &response->response.owTransfer;

                t->status = PROTO_OW_STATUS_OK;
                t->type   = PROTO_OW_TRANSFER_TYPE_UNKNOWN;
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
                    t->rxBuffer     = NULL;
                    t->rxBufferSize = 0;
                }
            }
            break;

        case PROTO_CMD_OW_TRANSFER:
            {
                ProtoResOwTransfer *t = &response->response.owTransfer;

                if (t->type == PROTO_OW_TRANSFER_TYPE_READ) {
                    t->data.transfer.data     = PTR_U8(memory) + 1;
                    t->data.transfer.dataSize = memorySize - 1;

                } else if (t->type == PROTO_OW_TRANSFER_TYPE_WRITE) {
                    t->data.transfer.data     = NULL;
                    t->data.transfer.dataSize = 0;
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

            case PROTO_CMD_OW_TRANSFER:
                {
                    ProtoResOwTransfer *t = &response->response.owTransfer;

                    PTR_U8(memory)[ret++] = t->status;

                    switch (t->type) {
                        case PROTO_OW_TRANSFER_TYPE_READ:
                            {
                                ret += t->data.transfer.dataSize;
                            }
                            break;

                        case PROTO_OW_TRANSFER_TYPE_SEARCH_STEP:
                            {
                                uint64_t romId = t->data.search.romId;

                                for (uint8_t i = 0; i < PROTO_OW_ROM_ID_SIZE; i++) {
                                    PTR_U8(memory)[ret + i] = romId;
                                    romId >>= 8;
                                }

                                ret += PROTO_OW_ROM_ID_SIZE;

                                PTR_U8(memory)[ret++] = t->data.search.descBit;
                                PTR_U8(memory)[ret++] = t->data.search.lastZero;
                            }
                            break;

                        default:
                            break;
                    }
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

            case PROTO_CMD_OW_TRANSFER:
                {
                    ProtoResOwTransfer *t = &response->response.owTransfer;

                    ret = memorySize != 0;
                    if (ret) {
                        t->status = *memoryP;

                        memoryP++;
                        memorySize--;
                    }

                    if (ret) {
                        if (t->status == PROTO_OW_STATUS_OK) {
                            if (t->type == PROTO_OW_TRANSFER_TYPE_READ) {
                                t->data.transfer.data     = memoryP;
                                t->data.transfer.dataSize = memorySize;
                            }

                        } else if (t->status == PROTO_OW_STATUS_SEARCH_STEP) {
                            ret = memorySize > PROTO_OW_ROM_ID_SIZE;
                            if (ret) {
                                for (uint8_t i = PROTO_OW_ROM_ID_SIZE; i > 0; i--) {
                                    t->data.search.romId <<= 8;
                                    t->data.search.romId |= memoryP[i - 1];
                                }

                                memoryP    += PROTO_OW_ROM_ID_SIZE;
                                memorySize -= PROTO_OW_ROM_ID_SIZE;
                            }

                            if (ret) {
                                ret = memorySize >= 2;
                                if (ret) {
                                    t->data.search.descBit   = memoryP[0];
                                    t->data.search.lastZero  = memoryP[1];

                                    memoryP    += 2;
                                    memorySize -= 2;
                                }
                            }
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
