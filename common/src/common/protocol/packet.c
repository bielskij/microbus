// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include "common/crc8.h"

#include "common/protocol.h"
#include "common/protocol/packet.h"

#include "common.h"


#define PTR_U8(x) ((uint8_t *)(x))

void proto_pkt_init(ProtoPkt *pkt, void *mem, uint16_t memSize, uint8_t code, uint8_t id) {
    const uint8_t footerSize = 1;
    uint8_t       headerSize = 3;

    bool ignorePacket = false;

    pkt->code = code;
    pkt->id   = id;

    if (mem && memSize) {
        uint16_t payloadSize = footerSize + headerSize;

        if (payloadSize > memSize) {
            ignorePacket = true;
            
        } else {
            payloadSize = memSize - payloadSize;
            if (payloadSize) {
                if (proto_int_val_length_estimate(payloadSize - 1) == 2) {
                    headerSize++;
                    payloadSize--;

                } else if (proto_int_val_length_estimate(payloadSize) == 2) {
                    payloadSize--;
                }
            }

            pkt->header      = mem;
            pkt->headerSize  = headerSize;

            pkt->payload     = pkt->header + pkt->headerSize;
            pkt->payloadSize = payloadSize;

            pkt->footer      = pkt->payload + pkt->payloadSize;
            pkt->footerSize  = footerSize;
        }

    } else {
        ignorePacket = true;
    }

    if (ignorePacket) {
        pkt->header      = NULL;
        pkt->headerSize  = 0;

        pkt->payload     = NULL;
        pkt->payloadSize = 0;
        
        pkt->footer      = NULL;
        pkt->footerSize  = 0;
    }

    proto_pkt_clear(pkt);
}

void proto_pkt_clear(ProtoPkt *pkt) {
	pkt->payloadUsed = 0;
	pkt->headerUsed  = 0;
    pkt->footerUsed  = 0;
}

bool proto_pkt_encode_header(ProtoPkt *pkt) {
	uint8_t off = 0;

	if (pkt->payloadUsed > pkt->payloadSize) {
		pkt->headerUsed = 0;

		return false;
	}

	pkt->header[off++] = PROTO_SYNC_NIBBLE | (pkt->code & PROTO_CMD_NIBBLE_MASK);
	pkt->header[off++] = pkt->id;

	pkt->headerUsed = off + proto_int_val_encode(pkt->payloadUsed, pkt->header + off);

	return true;
}

bool proto_pkt_encode(ProtoPkt *pkt) {
	bool ret = proto_pkt_encode_header(pkt);
	if (ret) {
		// crc
		pkt->footer[0] = crc8_get(pkt->header,  pkt->headerUsed,  PROTO_CRC8_POLY, PROTO_CRC8_START);
		pkt->footer[0] = crc8_get(pkt->payload, pkt->payloadUsed, PROTO_CRC8_POLY, pkt->footer[0]);

        pkt->footerUsed  = pkt->footerSize;

	} else {
		pkt->footer[0] = 0;
	}

	return ret;
}

uint16_t proto_pkt_size(ProtoPkt *pkt) {
	return pkt->headerSize + pkt->payloadSize + pkt->footerSize;
}
