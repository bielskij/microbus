// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef COMMON_PROTOCOL_PACKET_H_
#define COMMON_PROTOCOL_PACKET_H_

#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ProtoPkt {
	uint8_t   code;
	uint8_t   id;

	uint8_t  *header;
	uint8_t   headerSize;
	uint8_t   headerUsed;

	uint8_t  *payload;
	uint16_t  payloadSize;
	uint16_t  payloadUsed;

	uint8_t  *footer;
	uint8_t   footerSize;
	uint8_t   footerUsed;
} ProtoPkt;

void proto_pkt_init (ProtoPkt *pkt, void *mem, uint16_t memSize, uint8_t code, uint8_t id);
void proto_pkt_clear(ProtoPkt *pkt);

bool proto_pkt_encode_header(ProtoPkt *pkt);
bool proto_pkt_encode(ProtoPkt *pkt);

uint16_t proto_pkt_size(ProtoPkt *pkt);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_PROTOCOL_PACKET_H_ */
