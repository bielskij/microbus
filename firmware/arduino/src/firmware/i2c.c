// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <util/twi.h>

#include "firmware/utils.h"
#include "firmware/i2c.h"

#include "common/protocol/command.h"


#define TIMEOUT_CYCLES 60000

#define _i2cWait(cond, ret) { \
    uint16_t timer = 0; \
    \
    do { } while ((cond) && ++timer < TIMEOUT_CYCLES); \
    \
    if (timer == TIMEOUT_CYCLES) { \
        i2c_stop(); \
        i2c_initialize(); \
        return ret; \
    } \
}


#define _i2cExec(_flags) do { TWCR = _BV(TWINT) | _BV(TWEN) | (_flags); } while (0)

void i2c_initialize(void) {
    TWCR = 0;

    // 100kHZ
#if F_CPU == 8000000ULL
    TWBR = 32; // 100khz
    TWSR = 0;  // 1 - prescaler

    // TWBR = 2; // 400khz
    // TWSR = 0; // 1 - prescaler

#elif F_CPU == 16000000ULL
    TWBR = 72; // 100kHz
    TWSR = 0;  // 1 - prescaler

    // TWBR = 12; // 400kHz
    // TWSR = 0;  // 1 - prescaler
#else
    #error "Not supported value of F_CPU!"
#endif

    // Pull ups
    PIO_SET_HIGH(C, 4);
    PIO_SET_HIGH(C, 5);
}

uint8_t i2c_start() {
    _i2cExec(_BV(TWSTA));
    _i2cWait((TWCR & _BV(TWINT)) == 0, PROTO_I2C_STATUS_TIMEOUT);
    
    if (
        TW_STATUS != TW_START && 
        TW_STATUS != TW_REP_START
    ) {
        _i2cExec(0);

        return PROTO_I2C_STATUS_ARB_LOST;
    }

    return PROTO_I2C_STATUS_OK;
}

uint8_t i2c_stop() {
    _i2cExec(_BV(TWSTO));
    _i2cWait((TWCR & _BV(TWSTO)) != 0, PROTO_I2C_STATUS_TIMEOUT);

    return PROTO_I2C_STATUS_OK;
}

uint8_t i2c_readByte(uint8_t *b, bool ack) {
    _i2cExec(ack ? _BV(TWEA) : 0);
    _i2cWait((TWCR & _BV(TWINT)) == 0, PROTO_I2C_STATUS_TIMEOUT);

    if (ack) {
        if (TW_STATUS != TW_MR_DATA_ACK) {
            return PROTO_I2C_STATUS_ARB_LOST;
        }

    } else {
        if (TW_STATUS != TW_MR_DATA_NACK) {
            return PROTO_I2C_STATUS_ARB_LOST;
        }
    }
    
    *b = TWDR;

    return PROTO_I2C_STATUS_OK;
}

uint8_t i2c_writeByte(uint8_t b) {
    TWDR = b;

    _i2cExec(0);
    _i2cWait((TWCR & _BV(TWINT)) == 0, PROTO_I2C_STATUS_TIMEOUT);

    switch (TW_STATUS) {
        case TW_MR_DATA_NACK:
        case TW_MT_DATA_NACK:
            return PROTO_I2C_STATUS_NAK_DATA;

        case TW_MR_SLA_NACK:
        case TW_MT_SLA_NACK:
            return PROTO_I2C_STATUS_NAK_ADDRESS;

        case TW_MT_SLA_ACK:
        case TW_MT_DATA_ACK:
        case TW_MR_SLA_ACK:
        case TW_MR_DATA_ACK:
            return PROTO_I2C_STATUS_OK;

        default:
            return PROTO_I2C_STATUS_ARB_LOST;
    }
}
