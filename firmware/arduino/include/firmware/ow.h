// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef __FIRMWARE_OW_H__
#define __FIRMWARE_OW_H__

#include "common/types.h"

#define OW_SEARCH       0xF0
#define OW_SEARCH_ALARM 0xEC

typedef enum _OwDelay {
    OW_DELAY_TX_1_HI,
    OW_DELAY_TX_1_LO,
    OW_DELAY_TX_0_HI,
    OW_DELAY_TX_0_LO,

    OW_DELAY_RX_LO,
    OW_DELAY_RX_HI,
    OW_DELAY_RX_END,

    OW_DELAY_PRESENCE_LO,
    OW_DELAY_PRESENCE_HI,
    OW_DELAY_PRESENCE_END,
} OwDelay;

// | LOWUS | READUS | SAMPLE | HIUS |
// -_______--------------------------
typedef bool (*OwPioCallback)(uint16_t lowUs, uint16_t readUs, uint16_t hiUs);

void ow_initialize(OwPioCallback pioCallback);

void ow_terminate(void);

bool ow_presence(void);

bool ow_search_start(uint64_t *romId, uint8_t *descBit, uint8_t *lastZero, bool *wasLast);

bool ow_search_step(uint64_t *romId, uint8_t *descBit, uint8_t *lastZero, bool *wasLast);

void ow_byte_write(uint8_t byte);

// bool ow_detectPresence(void);

// _U8 bus_1wire_byteRead(void);

// void bus_1wire_byteWrite(_U8 byte);

// // Identification
// void bus_1wire_romRead(_U8 rom[8]);

// // Address specific device
// void bus_1wire_romMatch(_U8 rom[8]);

// // Skip addressing
// void bus_1wire_romSkip(void);

// // Obtain IDs of all devices on the bus
// _BOOL bus_1wire_romSearch(_U8 rom[8], _BOOL *isLast);

#endif /* DRV_BUS_1WIRE_H_ */
