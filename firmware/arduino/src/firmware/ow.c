// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include "firmware/ow.h"

static OwPioCallback pioCallback = NULL;

static void _writeBit(uint8_t bit) {
	if (bit) {
		pioCallback(64, 0, 6);

	} else {
		pioCallback(10, 0, 60);
	}
}

static uint8_t _readBit() {
	return pioCallback(6, 9, 55);
}

static uint8_t _triplet(bool bdir) {
	uint8_t ret;

	do {
		uint8_t id   = _readBit();
		uint8_t comp = _readBit();

		if (id && comp) {
			ret = 0x3; // error
			break;

		} else if (! id && ! comp) {
			ret = bdir ? 0x4 : 0;

		} else {
			bdir = id;
			ret  = id ? 0x05 : 0x02;
		}

		_writeBit(bdir);
	} while (0);

	return ret;
}

void ow_initialize(OwPioCallback _pioCallback) {
	pioCallback = _pioCallback;
}

void ow_terminate(void) {
	pioCallback = NULL;
}

bool ow_presence(void) {
	return pioCallback(480, 70, 410);
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

	ow_byte_write(OW_SEARCH);

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

void ow_byte_write(uint8_t byte) {
	uint8_t i;

	for (i = 0; i < 8; i++) {
		_writeBit(byte & 0x1);

		byte >>= 1;
	}
}


#if 0
#include "utils/utils.h"

#include "arch.h"
#include "arch/config.h"
#include "arch/drv/bus/1wire.h"

#define DEBUG_LEVEL 0
#include "arch/common/debug.h"


#define DELAY_US_WRITE_1_LOW  6
#define DELAY_US_WRITE_1_HIGH 64

#define DELAY_US_WRITE_0_LOW  60
#define DELAY_US_WRITE_0_HIGH 10

#define DELAY_US_READ_LOW  6
#define DELAY_US_READ_HIGH 9
#define DELAY_US_READ_END  55

#define DELAY_US_PRESENCE_LOW  490
#define DELAY_US_PRESENCE_HIGH 70
#define DELAY_US_PRESENCE_END  410

#define CMD_ROM_READ   0x33
#define CMD_ROM_SKIP   0xcc
#define CMD_ROM_MATCH  0x55
#define CMD_ROM_SEARCH 0xF0

#define _PIO_IN()  PIO_SET_INPUT (CONFIG_1_WIRE_PIO_BANK, CONFIG_1_WIRE_PIO_PIN);
#define _PIO_OUT() PIO_SET_OUTPUT(CONFIG_1_WIRE_PIO_BANK, CONFIG_1_WIRE_PIO_PIN);

#define _PIO_HIGH() PIO_SET_HIGH(CONFIG_1_WIRE_PIO_BANK, CONFIG_1_WIRE_PIO_PIN);
#define _PIO_LOW()  PIO_SET_LOW(CONFIG_1_WIRE_PIO_BANK, CONFIG_1_WIRE_PIO_PIN);

#define _PIO_IS_HIGH() PIO_IS_HIGH(CONFIG_1_WIRE_PIO_BANK, CONFIG_1_WIRE_PIO_PIN)

#define _INTERRUPT_DISABLE() {}
#define _INTERRUPT_ENABLE()  {}

#define __1WIRE_DELAY_US(us) _delay_us(us)


static void _writeBit(_U8 bit) {
	_INTERRUPT_DISABLE();
	{
		_PIO_OUT();
		_PIO_LOW();

		if (bit) {
			__1WIRE_DELAY_US(DELAY_US_WRITE_1_LOW);
		} else {
			__1WIRE_DELAY_US(DELAY_US_WRITE_0_LOW);
		}

		_PIO_IN();

		if (bit) {
			__1WIRE_DELAY_US(DELAY_US_WRITE_1_HIGH);
		} else {
			__1WIRE_DELAY_US(DELAY_US_WRITE_0_HIGH);
		}

	}
	_INTERRUPT_ENABLE();
}




static _S8 _readBit(void) {
	_S8 ret = 0;

	_INTERRUPT_DISABLE();
	{
		_PIO_OUT();
		_PIO_LOW();

		__1WIRE_DELAY_US(DELAY_US_READ_LOW);

		_PIO_IN();

		__1WIRE_DELAY_US(DELAY_US_READ_HIGH);

		if (_PIO_IS_HIGH()) {
			ret = 1;
		}

		__1WIRE_DELAY_US(DELAY_US_READ_END);
	}
	_INTERRUPT_ENABLE();

	return ret;
}




void bus_1wire_initialize(void) {
	_PIO_IN();
	_PIO_HIGH();
}




void bus_1wire_terminate(void) {
	_PIO_LOW();
	_PIO_IN();
}




_BOOL bus_1wire_detectPresence(void) {
	_BOOL ret = FALSE;

	_INTERRUPT_DISABLE();
	{
		_PIO_OUT();
		_PIO_LOW();

		__1WIRE_DELAY_US(DELAY_US_PRESENCE_LOW);

		_PIO_IN();

		__1WIRE_DELAY_US(DELAY_US_PRESENCE_HIGH);

		if (! _PIO_IS_HIGH()) {
			ret = TRUE;
		}

		__1WIRE_DELAY_US(DELAY_US_PRESENCE_END);
	}
	_INTERRUPT_ENABLE();

	return ret;
}




_U8 bus_1wire_byteRead(void) {
	_U8 ret = 0;
	_U8 i;

	for (i = 0; i < 8; i++) {
		ret >>= 1;

		if (_readBit()) {
			ret |= 0x80;
		}
	}

	return ret;
}


void bus_1wire_byteWrite(_U8 byte) {
	_U8 i;

	for (i = 0; i < 8; i++) {
		_writeBit(byte & 0x01);

		byte >>= 1;
	}
}


void bus_1wire_romRead(_U8 rom[8]) {
	_U8 i;

	bus_1wire_byteWrite(CMD_ROM_READ);

	for (i = 0; i < 8; i++) {
		rom[i] = bus_1wire_byteRead();
	}
}


void bus_1wire_romMatch(_U8 rom[8]) {
	_U8 i;

	bus_1wire_byteWrite(CMD_ROM_MATCH);

	for (i = 0; i < 8; i++) {
		bus_1wire_byteWrite(rom[i]);
	}
}


void bus_1wire_romSkip(void) {
	bus_1wire_byteWrite(CMD_ROM_SKIP);
}


static _U8 lastDeviation = 0;


_BOOL bus_1wire_romSearch(_U8 rom[8], _BOOL *isLast) {
	_BOOL ret = bus_1wire_detectPresence();

	if (ret) {
		_U8 newDeviation = 0;
		_U8 bitIndex     = 0;
		_U8 bitFirst;
		_U8 bitSecond;
		_U8 bitMask = 0x01;

		bus_1wire_byteWrite(CMD_ROM_SEARCH);

		do {
			bitFirst  = _readBit();
			bitSecond = _readBit();

			if (bitFirst == 1 && bitSecond == 1) {
				// ROM Search failed
				ret = FALSE;
				break;
			}

			if (bitFirst != bitSecond) {
				if (bitFirst) {
					*rom |= bitMask;

				} else {
					*rom &= ~bitMask;
				}

			} else if (bitIndex == lastDeviation) {
				*rom |= bitMask;

			} else if (bitIndex > lastDeviation) {
				*rom &= ~bitMask;

				newDeviation = bitIndex;

			} else if (! (*rom &bitMask)) {
				newDeviation = bitIndex;
			}

			_writeBit(*rom & bitMask);

			bitIndex++;

			bitMask <<= 1;
			if (! bitMask) {
				bitMask = 0x01;
				rom++;
			}
		} while (bitIndex < 64);

		lastDeviation = newDeviation;

		if (lastDeviation == 0) {
			*isLast = TRUE;
		}
	}

	return ret;
}
#endif