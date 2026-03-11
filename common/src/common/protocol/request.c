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

        case PROTO_CMD_OW_TRANSFER:
            {
                ProtoReqOwTransfer *t = &request->request.owTransfer;

                t->type = PROTO_OW_TRANSFER_TYPE_UNKNOWN;

                t->data.transfer.dataSize = memorySize - 1;
            }
            break;

        case PROTO_CMD_SPI_TRANSFER:
            {
                ProtoReqSpiTransfer *t = &request->request.spiTransfer;

                uint8_t overhead = 7;

                if (memorySize > 7) {
                    t->txBufferSize = memorySize - overhead;

                } else {
                    t->txBufferSize = 0;
                }

                t->txBuffer     = NULL;
                t->rxBufferSize = 0;
                t->rxBufferSize = 0;
                t->rxSkipSize   = 0;
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

        case PROTO_CMD_OW_TRANSFER:
            {
                ProtoReqOwTransfer *t = &request->request.owTransfer;

                uint8_t dataOffset = 1; // type

                switch (t->type) {
                    case PROTO_OW_TRANSFER_TYPE_WRITE:
                        t->data.transfer.data     = PTR_U8(memory) + dataOffset;
                        t->data.transfer.dataSize = memorySize - dataOffset;
                        break;

                    case PROTO_OW_TRANSFER_TYPE_READ:
                        t->data.transfer.data     = NULL;
                        t->data.transfer.dataSize = memorySize - dataOffset;
                        break;

                    default:
                        break;
                }
            }
            break;

        case PROTO_CMD_SPI_TRANSFER:
            {
                ProtoReqSpiTransfer *t = &request->request.spiTransfer;

                if (t->txBufferSize) {
                    t->txBuffer = PTR_U8(memory) + (1 + proto_int_val_length_estimate(t->txBufferSize));

                } else {
                    t->txBuffer = NULL;
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

            case PROTO_CMD_OW_TRANSFER:
                {
                    ProtoReqOwTransfer *t = &request->request.owTransfer;

                    PTR_U8(memory)[ret++] = t->type;

                    switch (t->type) {
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
                                PTR_U8(memory)[ret++] = t->data.search.type;
                            }
                            break;

                        case PROTO_OW_TRANSFER_TYPE_SEARCH_START:
                            {
                                PTR_U8(memory)[ret++] = t->data.search.type;
                            }
                            break;

                        case PROTO_OW_TRANSFER_TYPE_READ:
                            {
                                ret += proto_int_val_encode(t->data.transfer.dataSize, PTR_U8(memory) + ret);
                            }
                            break;

                        case PROTO_OW_TRANSFER_TYPE_WRITE:
                            {
                                ret += t->data.transfer.dataSize;
                            }
                            break;

                        case PROTO_OW_TRANSFER_TYPE_TOUCH_BIT:
                            {
                                PTR_U8(memory)[ret++] = t->data.touchBit.value;
                            }
                            break;

                        default:
                            break;
                    }
                }
                break;

            case PROTO_CMD_SPI_TRANSFER:
                {
                    ProtoReqSpiTransfer *t = &request->request.spiTransfer;

                    PTR_U8(memory)[ret++] = t->flags;

                    ret += proto_int_val_encode(t->txBufferSize, PTR_U8(memory) + ret);

                    ret += t->txBufferSize;

                    ret += proto_int_val_encode(t->rxSkipSize,   PTR_U8(memory) + ret);
                    ret += proto_int_val_encode(t->rxBufferSize, PTR_U8(memory) + ret);
                }
                break;

            default:
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

            case PROTO_CMD_OW_TRANSFER:
                {
                    ProtoReqOwTransfer *t = &request->request.owTransfer;

                    if (ret) {
                        ret = memorySize > 0;
                        if (ret) {
                            t->type = *memoryP;

                            memoryP++;
                            memorySize--;
                        }
                    }

                    if (ret) {
                        switch (t->type) {
                            case PROTO_OW_TRANSFER_TYPE_SEARCH_STEP:
                                {
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
                                            t->data.search.type      = memoryP[2];

                                            memoryP    += 3;
                                            memorySize -= 3;
                                        }
                                    }
                                }
                                break;

                            case PROTO_OW_TRANSFER_TYPE_SEARCH_START:
                                {
                                    ret = memorySize >= 1;
                                    if (ret) {
                                        t->data.search.type = memoryP[0];

                                        memoryP++;
                                        memorySize--;
                                    }
                                }
                                break;

                            case PROTO_OW_TRANSFER_TYPE_WRITE:
                                {
                                    t->data.transfer.dataSize = memorySize;
                                    t->data.transfer.data     = memoryP;
                                }
                                break;

                            case PROTO_OW_TRANSFER_TYPE_READ:
                                if (memorySize) {
                                    uint8_t lenSize = proto_int_val_length_probe(*memoryP);

                                    t->data.transfer.data = NULL;

                                    ret = memorySize >= lenSize;
                                    if (ret) {
                                        t->data.transfer.dataSize = proto_int_val_decode(memoryP);

                                        memoryP    += lenSize;
                                        memorySize -= lenSize;
                                    }
                                }
                                break;

                            case PROTO_OW_TRANSFER_TYPE_TOUCH_BIT:
                                if (memorySize) {
                                    t->data.touchBit.value = *memoryP;

                                    memoryP++;
                                    memorySize--;
                                }
                                break;
                        }
                    }
                }
                break;

            case PROTO_CMD_SPI_TRANSFER:
                {
                    ProtoReqSpiTransfer *t = &request->request.spiTransfer;

                    ret = memorySize > 0;
                    if (ret) {
                        t->flags = *memoryP;

                        memoryP++;
                        memorySize--;
                    }

                    {
                        uint8_t lenSize = proto_int_val_length_probe(*memoryP);

                        ret = lenSize <= memorySize;
                        if (ret) {
                            t->txBuffer     = NULL;
                            t->txBufferSize = proto_int_val_decode(memoryP);

                            memoryP    += lenSize;
                            memorySize -= lenSize;

                            if (t->txBufferSize) {
                                ret = t->txBufferSize <= memorySize;
                                if (ret) {
                                    t->txBuffer = memoryP;

                                    memoryP    += t->txBufferSize;
                                    memorySize -= t->txBufferSize;
                                }
                            }
                        }

                        if (ret) {
                            lenSize = proto_int_val_length_probe(*memoryP);

                            ret = lenSize <= memorySize;
                            if (ret) {
                                t->rxSkipSize = proto_int_val_decode(memoryP);

                                memoryP    += lenSize;
                                memorySize -= lenSize;
                            }
                        }

                        if (ret) {
                            lenSize = proto_int_val_length_probe(*memoryP);

                            ret = lenSize <= memorySize;
                            if (ret) {
                                t->rxBufferSize = proto_int_val_decode(memoryP);

                                memoryP    += lenSize;
                                memorySize -= lenSize;
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
