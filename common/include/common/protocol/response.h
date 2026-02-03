// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef COMMON_PROTOCOL_RESPONSE_H_
#define COMMON_PROTOCOL_RESPONSE_H_

#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ProtoResError {
	uint8_t code;
} ProtoResError;

typedef struct _ProtoResGetInfo {
	/// Protocol version
	struct {
		uint8_t major;
		uint8_t minor;
	} version;

	/// Maximal supported size of packet.
	uint16_t packetSize;

	uint8_t features;
} ProtoResGetInfo;

typedef struct _ProtoResI2cTransfer {
	uint8_t status;

	uint8_t *rxBuffer;
	uint16_t rxBufferSize;
} ProtoResI2cTransfer;

typedef struct _ProtoResOwTransfer {
    uint8_t  status;

    union {
        struct {

        } searchStart;

        struct {
            uint64_t romId;
            uint8_t  searchBit;
            uint8_t  descBit;
            uint8_t  lastZero;
        } searchStep;
    } data;
} ProtoResOwTransfer;

typedef struct _ProtoRes {
	uint8_t cmd;

	union {
		ProtoResGetInfo     getInfo;
		ProtoResI2cTransfer i2cTransfer;
        ProtoResOwTransfer  owTransfer;
	} response;
} ProtoRes;

void     proto_res_init  (ProtoRes *response, void *memory, uint16_t memorySize, uint8_t cmd);
void     proto_res_assign(ProtoRes *response, void *memory, uint16_t memorySize);
uint16_t proto_res_encode(ProtoRes *response, void *memory, uint16_t memorySize);
bool     proto_res_decode(ProtoRes *response, void *memory, uint16_t memorySize);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_PROTOCOL_RESPONSE_H_ */
