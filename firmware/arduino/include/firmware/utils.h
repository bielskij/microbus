// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef __FIRMWARE_UTILS_H__
#define __FIRMWARE_UTILS_H__

#include <avr/io.h>

#ifndef NULL
	#define NULL ((void *) 0)
#endif

#define CONCAT_MACRO(port, letter) port ## letter
#define DECLARE_PORT(port) CONCAT_MACRO(PORT, port)
#define DECLARE_DDR(port)  CONCAT_MACRO(DDR,  port)
#define DECLARE_PIN(port)  CONCAT_MACRO(PIN,  port)

#define PIO_SET_HIGH(bank, pin) { DECLARE_PORT(bank) |= _BV(DECLARE_PIN(pin)); }
#define PIO_SET_LOW(bank, pin)  { DECLARE_PORT(bank) &= ~_BV(DECLARE_PIN(pin)); }

#define PIO_TOGGLE(bank, pin) { DECLARE_PORT(bank) ^= _BV(DECLARE_PIN(pin)); }

#define PIO_IS_HIGH(bank, pin) (bit_is_set(DECLARE_PIN(bank), DECLARE_PIN(pin)))
#define PIO_IS_LOW(bank, pin) (! bit_is_set(DECLARE_PIN(bank), DECLARE_PIN(pin)))

#define PIO_SET_OUTPUT(bank, pin) { DECLARE_DDR(bank) |= _BV(DECLARE_PIN(pin)); }
#define PIO_SET_INPUT(bank, pin)  { DECLARE_DDR(bank) &= ~(_BV(DECLARE_PIN(pin))); }

#endif /* __FIRMWARE_UTILS_H__ */