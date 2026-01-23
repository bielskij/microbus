// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef __FIRMWARE_I2C_H__
#define __FIRMWARE_I2C_H__

#include <stdint.h>
#include <stdbool.h>

void i2c_initialize(void);

uint8_t i2c_start();

uint8_t i2c_stop();

uint8_t i2c_readByte(uint8_t *b, bool ack);

uint8_t i2c_writeByte(uint8_t b);

#endif /* __FIRMWARE_I2C_H__ */
