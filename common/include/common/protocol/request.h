// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef COMMON_PROTOCOL_REQUEST_H_
#define COMMON_PROTOCOL_REQUEST_H_

#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ProtoReqGetInfo {

} ProtoReqGetInfo;

typedef struct _ProtoReqI2CTransfer {
    uint8_t  flags;
    uint8_t  slaveAddress;

    uint8_t *data;
    uint16_t dataSize;
} ProtoReqI2CTransfer;

typedef struct _ProtoReqOwTransfer {
    uint8_t type;

    union {
        struct {
            uint8_t value;
        } touchBit;

        struct {
            uint8_t *data;
            uint16_t dataSize;
        } transfer;

        struct {
            uint64_t romId;
            uint8_t  descBit;
            uint8_t  lastZero;
            uint8_t  type;
        } search;
    } data;
} ProtoReqOwTransfer;

typedef struct _ProtoReqSpiTransfer {
    uint8_t *txBuffer;
    uint16_t txBufferSize;

    uint16_t rxBufferSize;
    uint16_t rxSkipSize;

    uint8_t flags;
} ProtoReqSpiTransfer;

typedef struct _ProtoReq {
    uint8_t cmd;

    union {
        ProtoReqGetInfo     getInfo;
        ProtoReqI2CTransfer i2cTransfer;
        ProtoReqOwTransfer  owTransfer;
        ProtoReqSpiTransfer spiTransfer;
    } request;
} ProtoReq;

void     proto_req_init  (ProtoReq *request, void *memory, uint16_t memorySize, uint8_t cmd);
void     proto_req_assign(ProtoReq *request, void *memory, uint16_t memorySize);
uint16_t proto_req_encode(ProtoReq *request, void *memory, uint16_t memorySize);
bool     proto_req_decode(ProtoReq *request, void *memory, uint16_t memorySize);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_PROTOCOL_REQUEST_H_ */
