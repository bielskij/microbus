// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef __FIRMWARE_UART_H__
#define __FIRMWARE_UART_H__

#include <stdint.h>

void uart_initialize(void);

void uart_send(char c);

char uart_poll(void);

uint8_t uart_get(void);

#endif /* __FIRMWARE_UART_H__ */