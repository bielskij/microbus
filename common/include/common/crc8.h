// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef COMMON_INCLUDE_CRC8_H_
#define COMMON_INCLUDE_CRC8_H_

#include "common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t crc8_getForByte(uint8_t byte, uint8_t polynomial, uint8_t start);

uint8_t crc8_get(uint8_t *buffer, uint16_t bufferSize, uint8_t polynomial, uint8_t start);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_INCLUDE_CRC8_H_ */
