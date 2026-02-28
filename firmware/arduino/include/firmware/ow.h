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

void ow_write(const uint8_t *data, uint16_t dataSize);

void ow_read(uint8_t *data, uint16_t dataSize);

bool ow_read_bit(void);

#endif /* DRV_BUS_1WIRE_H_ */
