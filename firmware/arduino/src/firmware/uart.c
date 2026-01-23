// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include "firmware/utils.h"
#include "firmware/uart.h"

/************************
 * UART
 */

#if BAUD >= 1000000
	#if F_CPU == 8000000
		#define F_CPU_PRESCALER 8
	#else
		#define F_CPU_PRESCALER 16
	#endif
#else
		#define F_CPU_PRESCALER 16
#endif


#define UART_BAUD_REG (((F_CPU / F_CPU_PRESCALER) / BAUD) - 1)

#define _waitForTransmit() while (! (UCSR0A & _BV(UDRE0)));

void uart_initialize() {
	// Configure usart
	UBRR0H = ((UART_BAUD_REG) >> 8);
	UBRR0L = ((UART_BAUD_REG) & 0x00FF);

#if F_CPU_PRESCALER == 8
	UCSR0A |= _BV(U2X0);
#endif

	// 8bit, 1bit stop, no parity
	UCSR0C  = _BV(UCSZ00) | _BV(UCSZ01);

	// enable
	UCSR0B |= (_BV(TXEN0) | _BV(RXEN0));
}

void uart_send(char c) {
	_waitForTransmit();

	UDR0 = c;
}

char uart_poll() {
	if (UCSR0A & _BV(RXC0)) {
		return 1;
	}

	return 0;
}

uint8_t uart_get() {
	return UDR0;
}