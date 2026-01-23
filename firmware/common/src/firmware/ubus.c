// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include "common/protocol.h"
#include "common/protocol/command.h"

#include "firmware/ubus.h"

static void _sendError(UbusHub *hub, uint8_t errorCode) {
    proto_pkt_clear(&hub->packet);

    hub->packet.code = errorCode;
}

void ubus_hub_setup(
    UbusHub                *hub,
    uint8_t                *memory,
    uint16_t                memorySize,
    UbusHubRequestCallback  requestCallback,
    UbusHubResponseCallback responseCallback,
    void                   *callbackData
) {
    hub->requestCallback  = requestCallback;
    hub->responseCallback = responseCallback;
    hub->callbackData     = callbackData;

    proto_pkt_init(&hub->packet, memory, memorySize, 0, 0);

    proto_pkt_dec_reset(&hub->packetDeserializer, &hub->packet);
}

void ubus_hub_putByte(UbusHub *hub, uint8_t byte) {
    ProtoPkt *pkt = &hub->packet;

    uint8_t ret = proto_pkt_dec_putByte(&hub->packetDeserializer, byte, pkt);
    if (ret != PROTO_PKT_DES_RET_IDLE) {
        do {
            ProtoRes response;
            ProtoReq request;

            if (PROTO_PKT_DES_RET_GET_ERROR_CODE(ret) != PROTO_NO_ERROR) {
                _sendError(hub, PROTO_PKT_DES_RET_GET_ERROR_CODE(ret));
                break;
            }

            // Parse, assign request to coming packet
            proto_req_init(&request, pkt->payload, pkt->payloadUsed, pkt->code);

            if (! proto_req_decode(&request, pkt->payload, pkt->payloadUsed)) {
                _sendError(hub, PROTO_ERROR_INVALID_MESSAGE);
                break;
            }
            
            {
                proto_res_init(&response, pkt->payload, pkt->payloadSize, pkt->code);

                // Start preparation of response
                proto_pkt_clear(pkt);

                pkt->code = PROTO_NO_ERROR;
            }

            switch (request.cmd) {
                case PROTO_CMD_GET_INFO:
                    {
                        ProtoResGetInfo *res = &response.response.getInfo;

                        res->version.major = PROTO_VERSION_MAJOR;
                        res->version.minor = PROTO_VERSION_MINOR;

                        res->packetSize = proto_pkt_size(pkt);
                    }
                    break;

                case PROTO_CMD_I2C_TRANSFER:
                    {
                        ProtoResI2cTransfer *res = &response.response.i2cTransfer;

                        if (res->rxBufferSize < request.request.i2cTransfer.dataSize) {
                            _sendError(hub, PROTO_ERROR_INVALID_MESSAGE);
                            break;

                        } else {
                            if (request.request.i2cTransfer.flags & PROTO_I2C_TRANSFER_FLAG_READ) {
                                res->rxBufferSize = request.request.i2cTransfer.dataSize;
                                
                            } else {
                                res->rxBufferSize = 0;
                            }
                        }
                    }
                    break;

                default:
                    _sendError(hub, PROTO_ERROR_INVALID_CMD);
                    break;
            }

            if (pkt->code != PROTO_NO_ERROR) {
                break;
            }

            proto_res_assign(&response, pkt->payload, pkt->payloadSize);
            {
                hub->requestCallback(&request, &response, hub->callbackData);
            }
            pkt->payloadUsed = proto_res_encode(&response, pkt->payload, pkt->payloadSize);

        } while (0);

        proto_pkt_encode(pkt);

        hub->responseCallback(pkt->header,  pkt->headerUsed,  hub->callbackData);
        hub->responseCallback(pkt->payload, pkt->payloadUsed, hub->callbackData);
        hub->responseCallback(pkt->footer,  pkt->footerUsed,  hub->callbackData);

        proto_pkt_dec_reset(&hub->packetDeserializer, &hub->packet);
    }
}

void ubus_hub_reset(UbusHub *hub) {
    proto_pkt_dec_reset(&hub->packetDeserializer, &hub->packet);
}