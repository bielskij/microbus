// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <gtest/gtest.h>

#include "common/crc8.h"
#include "common/protocol.h"

TEST(crc8, getForByte_zero) {
	ASSERT_EQ(crc8_getForByte(0x00, 0x07, 0x00), 0x00);
}

TEST(crc8, getForByte_single_byte) {
	ASSERT_EQ(crc8_getForByte(0x01, 0x07, 0x00), 0x01);
}

TEST(crc8, getForByte_with_start_value) {
	uint8_t result = crc8_getForByte(0x00, 0x07, 0x07);
	ASSERT_NE(result, 0x00);
}

TEST(crc8, getForByte_all_zeros) {
	ASSERT_EQ(crc8_getForByte(0x00, PROTO_CRC8_POLY, PROTO_CRC8_START), 0x00);
}

TEST(crc8, getForByte_all_ones) {
	uint8_t result = crc8_getForByte(0xff, PROTO_CRC8_POLY, PROTO_CRC8_START);
	ASSERT_NE(result, 0x00);
}

TEST(crc8, getForByte_deterministic) {
	ASSERT_EQ(crc8_getForByte(0x55, PROTO_CRC8_POLY, PROTO_CRC8_START),
	          crc8_getForByte(0x55, PROTO_CRC8_POLY, PROTO_CRC8_START));
}

TEST(crc8, get_empty_buffer) {
	uint8_t buffer[1];
	ASSERT_EQ(crc8_get(buffer, 0, PROTO_CRC8_POLY, PROTO_CRC8_START), PROTO_CRC8_START);
}

TEST(crc8, get_single_byte) {
	uint8_t buffer[] = { 0x01 };
	uint8_t expected = crc8_getForByte(0x01, PROTO_CRC8_POLY, PROTO_CRC8_START);
	ASSERT_EQ(crc8_get(buffer, 1, PROTO_CRC8_POLY, PROTO_CRC8_START), expected);
}

TEST(crc8, get_multiple_bytes) {
	uint8_t buffer[] = { 0x01, 0x02, 0x03, 0x04 };

	uint8_t expected = PROTO_CRC8_START;
	for (int i = 0; i < 4; i++) {
		expected = crc8_getForByte(buffer[i], PROTO_CRC8_POLY, expected);
	}

	ASSERT_EQ(crc8_get(buffer, 4, PROTO_CRC8_POLY, PROTO_CRC8_START), expected);
}

TEST(crc8, get_same_data_same_result) {
	uint8_t buffer1[] = { 0xde, 0xad, 0xbe, 0xef };
	uint8_t buffer2[] = { 0xde, 0xad, 0xbe, 0xef };

	ASSERT_EQ(crc8_get(buffer1, 4, PROTO_CRC8_POLY, PROTO_CRC8_START),
	          crc8_get(buffer2, 4, PROTO_CRC8_POLY, PROTO_CRC8_START));
}

TEST(crc8, get_different_data_different_result) {
	uint8_t buffer1[] = { 0x00, 0x00, 0x00 };
	uint8_t buffer2[] = { 0x01, 0x00, 0x00 };

	ASSERT_NE(crc8_get(buffer1, 3, PROTO_CRC8_POLY, PROTO_CRC8_START),
	          crc8_get(buffer2, 3, PROTO_CRC8_POLY, PROTO_CRC8_START));
}

TEST(crc8, get_with_different_polynomials) {
	uint8_t buffer[] = { 0x55 };

	uint8_t crc1 = crc8_get(buffer, 1, 0x07, PROTO_CRC8_START);
	uint8_t crc2 = crc8_get(buffer, 1, 0x31, PROTO_CRC8_START);

	ASSERT_NE(crc1, crc2);
}

TEST(crc8, get_with_different_start_values) {
	uint8_t buffer[] = { 0x55 };

	uint8_t crc1 = crc8_get(buffer, 1, PROTO_CRC8_POLY, 0x00);
	uint8_t crc2 = crc8_get(buffer, 1, PROTO_CRC8_POLY, 0xff);

	ASSERT_NE(crc1, crc2);
}

TEST(crc8, get_pattern_test) {
	uint8_t buffer[256];

	for (int i = 0; i < 256; i++) {
		buffer[i] = (uint8_t)i;
	}

	uint8_t crc = crc8_get(buffer, 256, PROTO_CRC8_POLY, PROTO_CRC8_START);
	ASSERT_NE(crc, PROTO_CRC8_START);
}
