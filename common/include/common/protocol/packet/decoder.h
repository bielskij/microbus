// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef COMMON_PROTOCOL_DECODER_H_
#define COMMON_PROTOCOL_DECODER_H_

#include "common/protocol/packet.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_PKT_DES_RET_IDLE 0
#define PROTO_PKT_DES_RET_GET_ERROR_CODE(_v)((_v) & 0x7F)
#define PROTO_PKT_DES_RET_SET_ERROR_CODE(_v)((_v) | 0x80)

typedef struct _ProtoPktDesCtx {
	uint8_t  state;
	uint16_t dataRead;
} ProtoPktDec;

uint8_t proto_pkt_dec_putByte(ProtoPktDec *ctx, uint8_t byte, ProtoPkt *pkt);

void proto_pkt_dec_reset(ProtoPktDec *ctx, ProtoPkt *pkt);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_PROTOCOL_DECODER_H_ */
