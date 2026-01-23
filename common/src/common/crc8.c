// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include "common/crc8.h"

uint8_t crc8_getForByte(uint8_t byte, uint8_t polynomial, uint8_t start) {
	uint8_t remainder = start;

	remainder ^= byte;

	{
		uint8_t bit;

		for (bit = 0; bit < 8; bit++) {
			if (remainder & 0x01) {
				remainder = (remainder >> 1) ^ polynomial;

			} else {
				remainder = (remainder >> 1);
			}

		}
	}

	return remainder;
}

uint8_t crc8_get(uint8_t *buffer, uint16_t bufferSize, uint8_t polynomial, uint8_t start) {
	uint8_t remainder = start;
	uint16_t byte;

	// Perform modulo-2 division, a byte at a time.
	for (byte = 0; byte < bufferSize; ++byte) {
		remainder = crc8_getForByte(buffer[byte], polynomial, remainder);
	}

	return remainder;
}
