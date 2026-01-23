// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef __UBUS_DEVICE_H__
#define __UBUS_DEVICE_H__

#include "common/protocol/packet.h"
#include "common/protocol/packet/decoder.h"
#include "common/protocol/request.h"
#include "common/protocol/response.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _UbusBuffer {
    uint8_t *data;
    uint16_t dataSize;
} UbusBuffer;

typedef void (*UbusHubRequestCallback)(ProtoReq *request, ProtoRes *response, void *callbackData);
typedef void (*UbusHubResponseCallback)(uint8_t *buffer, uint16_t bufferSize, void *callbackData);

typedef struct _UbusHub {
    ProtoPkt    packet;
    ProtoPktDec packetDeserializer;

    UbusHubRequestCallback  requestCallback;
    UbusHubResponseCallback responseCallback;
    void                   *callbackData;
} UbusHub;

void ubus_hub_setup(
	UbusHub                *hub,
	uint8_t                *memory,
	uint16_t                memorySize,
	UbusHubRequestCallback  requestCallback,
	UbusHubResponseCallback responseCallback,
	void                   *callbackData
);

void ubus_hub_putByte(UbusHub *hub, uint8_t byte);

void ubus_hub_reset(UbusHub *hub);

#ifdef __cplusplus
}
#endif

#endif /* __UBUS_DEVICE_H__ */