// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <gtest/gtest.h>

#include "common/protocol/common.h"

TEST(proto_int_val, length_estimate_zero) {
	ASSERT_EQ(proto_int_val_length_estimate(0), 1);
}

TEST(proto_int_val, length_estimate_small) {
	ASSERT_EQ(proto_int_val_length_estimate(1), 1);
	ASSERT_EQ(proto_int_val_length_estimate(127), 1);
}

TEST(proto_int_val, length_estimate_large) {
	ASSERT_EQ(proto_int_val_length_estimate(128), 2);
	ASSERT_EQ(proto_int_val_length_estimate(255), 2);
	ASSERT_EQ(proto_int_val_length_estimate(32767), 2);
}

TEST(proto_int_val, length_estimate_max) {
	ASSERT_EQ(proto_int_val_length_estimate(PROTO_INT_VAL_MAX), 2);
}

TEST(proto_int_val, length_probe_single_byte) {
	ASSERT_EQ(proto_int_val_length_probe(0x00), 1);
	ASSERT_EQ(proto_int_val_length_probe(0x7f), 1);
}

TEST(proto_int_val, length_probe_double_byte) {
	ASSERT_EQ(proto_int_val_length_probe(0x80), 2);
	ASSERT_EQ(proto_int_val_length_probe(0xff), 2);
}

TEST(proto_int_val, encode_zero) {
	uint8_t encoded[2];

	uint8_t len = proto_int_val_encode(0, encoded);

	ASSERT_EQ(len, 1);
	ASSERT_EQ(encoded[0], 0x00);
}

TEST(proto_int_val, encode_small_value) {
	uint8_t encoded[2];

	uint8_t len = proto_int_val_encode(100, encoded);

	ASSERT_EQ(len, 1);
	ASSERT_EQ(encoded[0], 100);
}

TEST(proto_int_val, encode_boundary_127) {
	uint8_t encoded[2];

	uint8_t len = proto_int_val_encode(127, encoded);

	ASSERT_EQ(len, 1);
	ASSERT_EQ(encoded[0], 127);
}

TEST(proto_int_val, encode_boundary_128) {
	uint8_t encoded[2];

	uint8_t len = proto_int_val_encode(128, encoded);

	ASSERT_EQ(len, 2);
	ASSERT_EQ(encoded[0], 0x80);
	ASSERT_EQ(encoded[1], 128);
}

TEST(proto_int_val, encode_256) {
	uint8_t encoded[2];

	uint8_t len = proto_int_val_encode(256, encoded);

	ASSERT_EQ(len, 2);
	ASSERT_EQ(encoded[0], 0x81);
	ASSERT_EQ(encoded[1], 0x00);
}

TEST(proto_int_val, encode_max) {
	uint8_t encoded[2];

	uint8_t len = proto_int_val_encode(PROTO_INT_VAL_MAX, encoded);

	ASSERT_EQ(len, 2);
	ASSERT_EQ(encoded[0], 0xff);
	ASSERT_EQ(encoded[1], 0xff);
}

TEST(proto_int_val, decode_single_byte) {
	uint8_t encoded[] = { 0x42, 0x00 };

	uint16_t value = proto_int_val_decode(encoded);

	ASSERT_EQ(value, 0x42);
}

TEST(proto_int_val, decode_double_byte) {
	uint8_t encoded[] = { 0x81, 0x2c };

	uint16_t value = proto_int_val_decode(encoded);

	ASSERT_EQ(value, 0x12c);
}

TEST(proto_int_val, decode_max) {
	uint8_t encoded[] = { 0xff, 0xff };

	uint16_t value = proto_int_val_decode(encoded);

	ASSERT_EQ(value, PROTO_INT_VAL_MAX);
}

TEST(proto_int_val, encode_decode_roundtrip) {
	uint16_t testValues[] = { 0, 1, 50, 100, 127, 128, 200, 256, 500, 1000, 5000, 10000, 32767 };

	for (auto val : testValues) {
		uint8_t encoded[2];
		uint8_t encodeLen = proto_int_val_encode(val, encoded);
		uint16_t decoded = proto_int_val_decode(encoded);

		ASSERT_EQ(decoded, val) << "Failed for value " << val;

		if (val <= 127) {
			ASSERT_EQ(encodeLen, 1) << "Failed length check for value " << val;
		} else {
			ASSERT_EQ(encodeLen, 2) << "Failed length check for value " << val;
		}
	}
}
