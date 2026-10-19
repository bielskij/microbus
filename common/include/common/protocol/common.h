// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef COMMON_PROTOCOL_COMMON_H_
#define COMMON_PROTOCOL_COMMON_H_

#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_INT_VAL_MAX 0x7fff

uint8_t proto_int_val_length_estimate(uint16_t val);

uint8_t proto_int_val_length_probe(uint8_t byte);

uint16_t proto_int_val_decode(uint8_t val[2]);

uint8_t proto_int_val_encode(uint16_t len, uint8_t val[2]);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_PROTOCOL_COMMON_H_ */
