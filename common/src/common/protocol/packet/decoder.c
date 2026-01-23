// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include "common/crc8.h"

#include "common/protocol.h"
#include "common/protocol/packet/decoder.h"

#include "../common.h"


typedef enum _State {
	STATE_WAIT_SYNC,
	STATE_ID,
	STATE_VLEN_HI,
	STATE_VLEN_LO,
	STATE_CHECK_PAYLOAD,
	STATE_PAYLOAD,
	STATE_CRC,
	STATE_CMD_RDY
} State;

uint8_t proto_pkt_dec_putByte(ProtoPktDec *ctx, uint8_t byte, ProtoPkt *pkt) {
	uint8_t error = PROTO_NO_ERROR;

	switch (ctx->state) {
		case STATE_WAIT_SYNC:
			{
				if ((byte & PROTO_SYNC_NIBBLE_MASK) == PROTO_SYNC_NIBBLE) {
					pkt->code = PROTO_CMD_NIBBLE_MASK & byte;

					ctx->state = STATE_ID;
				}
			}
			break;

		case STATE_ID:
			{
				pkt->id  = byte;

				ctx->state = STATE_VLEN_HI;
			}
			break;

		case STATE_VLEN_HI:
			{
				if (proto_int_val_length_probe(byte) == 1) {
					pkt->payloadUsed = byte;

					if (pkt->payloadUsed == 0) {
						ctx->state = STATE_CRC;

					} else {
						ctx->state = STATE_CHECK_PAYLOAD;
					}

				} else {
					pkt->header[0] = byte;

					ctx->state = STATE_VLEN_LO;
				}
			}
			break;

		case STATE_VLEN_LO:
			{
				pkt->header[1] = byte;

				pkt->payloadUsed = proto_int_val_decode(pkt->header);

				ctx->state = STATE_CHECK_PAYLOAD;
			}
			break;

		case STATE_PAYLOAD:
			{
				pkt->payload[ctx->dataRead++] = byte;

				if (ctx->dataRead == pkt->payloadUsed) {
					ctx->state = STATE_CRC;
				}
			}
			break;

		case STATE_CRC:
			{
				uint8_t calculatedCrc;

				pkt->footer[0] = byte;

				proto_pkt_encode_header(pkt);

				calculatedCrc = crc8_get(pkt->header,  pkt->headerUsed,  PROTO_CRC8_POLY, PROTO_CRC8_START);
				calculatedCrc = crc8_get(pkt->payload, pkt->payloadUsed, PROTO_CRC8_POLY, calculatedCrc);
				
				if (calculatedCrc != pkt->footer[0]) {
					error = PROTO_ERROR_INVALID_CRC;

				} else {	
					ctx->state = STATE_CMD_RDY;
				}
			}
			break;
	}

	if (ctx->state == STATE_CHECK_PAYLOAD) {
		if (pkt->payloadUsed > pkt->payloadSize) {
			error = PROTO_ERROR_INVALID_LENGTH;

		} else {
			ctx->state = STATE_PAYLOAD;
		}
	}

	{
		uint8_t ret = PROTO_PKT_DES_RET_IDLE;

		if (error != PROTO_NO_ERROR) {
			ret = PROTO_PKT_DES_RET_SET_ERROR_CODE(error);

		} else if (ctx->state == STATE_CMD_RDY) {
			ret = PROTO_PKT_DES_RET_SET_ERROR_CODE(error);
		}

		return ret;
	}
}

void proto_pkt_dec_reset(ProtoPktDec *ctx, ProtoPkt *pkt) {
	ctx->state    = STATE_WAIT_SYNC;
	ctx->dataRead = 0;
	
	if (pkt) {
		proto_pkt_clear(pkt);
	}
}
