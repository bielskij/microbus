// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include "firmware/ow.h"

static OwPioCallback pioCallback = NULL;

static void _writeBit(uint8_t bit) {
	if (bit) {
		pioCallback(10, 0, 60);

	} else {
		pioCallback(64, 0, 6);
	}
}

static bool _readBit() {
	return pioCallback(6, 9, 55);
}

static uint8_t _triplet(bool bdir) {
	uint8_t ret;

	do {
		bool id   = _readBit();
		bool comp = _readBit();

		if (id && comp) {
			ret = 0x3; // error
			break;
		}

		if (! id && ! comp) {
			ret = bdir ? 0x4 : 0;

		} else {
			bdir = id;
			ret  = id ? 0x05 : 0x02;
		}

		_writeBit(bdir);
	} while (0);

	return ret;
}

static void _writeByte(uint8_t byte) {
	uint8_t i;

	for (i = 0; i < 8; i++) {
		_writeBit(byte & 0x01);

		byte >>= 1;
	}
}

static uint8_t _readByte() {
	uint8_t ret = 0;

	for (uint8_t i = 0; i < 8; i++) {
		ret |= (_readBit() << i);
	}

	return ret;
}

void ow_initialize(OwPioCallback _pioCallback) {
	pioCallback = _pioCallback;
}

void ow_terminate(void) {
	pioCallback = NULL;
}

bool ow_presence(void) {
	return ! pioCallback(480, 70, 410);
}

bool ow_search_start(uint64_t *romId, uint8_t *descBit, uint8_t *lastZero, bool *wasLast) {
	*romId = 0;

	*descBit  = 64;
	*lastZero = -1;
	*wasLast  = false;

	return ow_search_step(romId, descBit, lastZero, wasLast);
}

// Search algoritm implementation inspired by u-boot/v2025.01/source/drivers/w1/w1-uclass.c
bool ow_search_step(uint64_t *romId, uint8_t *descBit, uint8_t *lastZero, bool *wasLast) {
	uint64_t lastRomId = *romId;
	uint8_t  searchBit = 0;
	uint8_t  tripletRet;
	uint64_t tmp;

	_writeByte(OW_SEARCH);

	*romId = 0;

	for (uint8_t i = 0; i < 64; i++) {
		if (i == *descBit) {
			searchBit = 1;

		} else if (i > *descBit) {
			searchBit = 0;

		} else {
			searchBit = (lastRomId >> i) & 0x1;
		}

		tripletRet = _triplet(searchBit);

		if ((tripletRet & 0x03) == 0x03) {
			break;
		}

		if (tripletRet == 0) {
			*lastZero = i;
		}

		tmp = (tripletRet >> 2);
		*romId |= (tmp << i);
	}

	if ((tripletRet & 0x03) != 0x03) {
		if (*descBit == *lastZero || *lastZero < 0) {
			*wasLast = true;
		}

		*descBit = *lastZero;

		return true;
	}

	return false;
}

void ow_write(const uint8_t *data, uint16_t dataSize) {
	for (uint16_t i = 0; i < dataSize; i++) {
		_writeByte(data[i]);
	}
}

void ow_read(uint8_t *data, uint16_t dataSize) {
	for (uint16_t i = 0; i < dataSize; i++) {
		data[i] = _readByte();
	}
}

bool ow_read_bit(void) {
	return _readBit();
}