// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <util/twi.h>
#include <util/delay.h>

#include "common/protocol.h"
#include "common/protocol/command.h"

#include "firmware/utils.h"
#include "firmware/ubus.h"
#include "firmware/uart.h"
#include "firmware/i2c.h"
#include "firmware/ow.h"


#define OW_PIO_BANK C
#define OW_PIO_PIN  0

#define DATA_BUFFER_SIZE 512

static uint8_t _dataBuffer[DATA_BUFFER_SIZE] = { 0 };


static void _ubusRequestCallback(ProtoReq *request, ProtoRes *response, void *callbackData) {
    switch (request->cmd) {
        case PROTO_CMD_GET_INFO:
            {
                ProtoResGetInfo *info = &response->response.getInfo;

                info->features = PROTO_FEATURE_I2C | PROTO_FEATURE_OW;
            }
            break;

        case PROTO_CMD_I2C_TRANSFER:
            {
                ProtoReqI2CTransfer *req = &request->request.i2cTransfer;
                ProtoResI2cTransfer *res = &response->response.i2cTransfer;

                // Start
                if (
                    req->flags & PROTO_I2C_TRANSFER_FLAG_START ||
                    req->flags & PROTO_I2C_TRANSFER_FLAG_REPEATED_START
                ) {
                    res->status = i2c_start();

                    // Address
                    if (res->status == PROTO_I2C_STATUS_OK) {
                        uint8_t address = req->slaveAddress << 1;

                        if (req->flags & PROTO_I2C_TRANSFER_FLAG_READ) {
                            address |= TW_READ;

                        } else {
                            address |= TW_WRITE;
                        }

                        res->status = i2c_writeByte(address);
                    }
                }

                // Data
                if (res->status == PROTO_I2C_STATUS_OK) {
                    uint16_t i = 0;

                    if (req->flags & PROTO_I2C_TRANSFER_FLAG_READ) {
                        while ((i < req->dataSize) && (res->status == PROTO_I2C_STATUS_OK)) {
                            bool ack;

                            if (req->flags & PROTO_I2C_TRANSFER_FLAG_CONT) {
                                ack = true;

                            } else {
                                ack = i != (req->dataSize - 1);
                            }

                            res->status = i2c_readByte(&res->rxBuffer[i], ack);

                            i++;
                        }

                    } else {
                        while ((i < req->dataSize) && (res->status == PROTO_I2C_STATUS_OK)) {
                            res->status = i2c_writeByte(req->data[i++]);
                        }

                        res->rxBufferSize = 0;
                    }
                }

                if (res->status != PROTO_I2C_STATUS_OK) {
                    res->rxBufferSize = 0;
                }

                if (
                    req->flags & PROTO_I2C_TRANSFER_FLAG_STOP ||
                    res->status != PROTO_I2C_STATUS_OK
                ) {
                    i2c_stop();
                }
            }
            break;

        case PROTO_CMD_OW_TRANSFER:
            {
                ProtoReqOwTransfer *req = &request->request.owTransfer;
                ProtoResOwTransfer *res = &response->response.owTransfer;

                res->type = req->type;

                switch (req->type) {
                    case PROTO_OW_TRANSFER_TYPE_RESET:
                        {
                            if (ow_presence()) {
                                res->status = PROTO_OW_STATUS_OK;

                            } else {
                                res->status = PROTO_OW_STATUS_NO_PRESENCE;
                            }
                        }
                        break;

                    case PROTO_OW_TRANSFER_TYPE_SEARCH_START:
                    case PROTO_OW_TRANSFER_TYPE_SEARCH_STEP:
                        {
                            if (! ow_presence()) {
                                res->status = PROTO_OW_STATUS_SEARCH_DONE_EMPTY;

                            } else {
                                bool last;
                                bool searchRet;

                                if (req->type == PROTO_OW_TRANSFER_TYPE_SEARCH_START) {
                                    searchRet = ow_search_start(
                                        &res->data.search.romId,
                                        &res->data.search.descBit,
                                        &res->data.search.lastZero,
                                        &last
                                    );

                                } else {
                                    res->data.search.romId    = req->data.search.romId;
                                    res->data.search.descBit  = req->data.search.descBit;
                                    res->data.search.lastZero = req->data.search.lastZero;

                                    searchRet = ow_search_step(
                                        &res->data.search.romId,
                                        &res->data.search.descBit,
                                        &res->data.search.lastZero,
                                        &last
                                    );
                                }

                                if (searchRet) {
                                    if (last) {
                                        res->status = PROTO_OW_STATUS_SEARCH_DONE_FOUND;

                                    } else {
                                        res->status = PROTO_OW_STATUS_SEARCH_STEP;
                                    }

                                } else {
                                    res->status = PROTO_OW_STATUS_SEARCH_DONE_EMPTY;
                                }
                            }

                        }
                        break;
                }
            }
            break;

        default:
            break;
    }
}

static void _ubusResponseCallback(uint8_t *buffer, uint16_t bufferSize, void *callbackData) {
    for (uint16_t i = 0; i < bufferSize; i++) {
        uart_send(buffer[i]);
    }
}

#define TIMER_PRESCALLER 8

#if ((F_CPU / TIMER_PRESCALLER) % 1000000UL) != 0
    #error "Timer prescaler does not produce an integer number of ticks per microsecond"
#else
    #define TIMER_TICKS_PER_US ((F_CPU / TIMER_PRESCALLER) / 1000000UL)
#endif

#if TIMER_TICKS_PER_US == 0
    #error "TIMER_TICKS_PER_US evaluates to 0 - check F_CPU and TIMER_PRESCALER"
#endif

#if TIMER_TICKS_PER_US == 0
    #define TIMER_TICKS_PER_US_SHIFT 0
#elif TIMER_TICKS_PER_US == 2
    #define TIMER_TICKS_PER_US_SHIFT 1
#elif TIMER_TICKS_PER_US == 4
    #define TIMER_TICKS_PER_US_SHIFT 2
#elif TIMER_TICKS_PER_US == 8
    #define TIMER_TICKS_PER_US_SHIFT 3
#elif TIMER_TICKS_PER_US == 16
    #define TIMER_TICKS_PER_US_SHIFT 4
#else
    #define TIMER_TICKS_PER_US_SHIFT -1
#endif

static void _timerWait(uint16_t delayUs) {
    uint16_t current = TCNT1;

#if TIMER_TICKS_PER_US_SHIFT >= 0
    delayUs <<= TIMER_TICKS_PER_US_SHIFT;
#else
    delayUs *= TIMER_TICKS_PER_US;
#endif

    uint16_t target = current + delayUs;

    while (((int16_t) (TCNT1 - target)) < 0);
}

static bool _owPioCallback(uint16_t lowUs, uint16_t readUs, uint16_t hiUs) {
    bool ret;

    // LO
    PIO_SET_LOW   (OW_PIO_BANK, OW_PIO_PIN);
    PIO_SET_OUTPUT(OW_PIO_BANK, OW_PIO_PIN);

    _timerWait(lowUs);

    PIO_SET_INPUT(OW_PIO_BANK, OW_PIO_PIN);
    PIO_SET_HIGH (OW_PIO_BANK, OW_PIO_PIN);

    if (readUs) {
        _timerWait(readUs);
    }

    ret = PIO_IS_HIGH(OW_PIO_BANK, OW_PIO_PIN);

    if (hiUs) {
        _timerWait(hiUs);
    }

    return ret;
}

int main(int argc, char *argv[]) {
    UbusHub ubusHub;

    uart_initialize();
    i2c_initialize();

    {
        PIO_SET_INPUT(OW_PIO_BANK, OW_PIO_PIN);
        PIO_SET_HIGH(OW_PIO_BANK, OW_PIO_PIN);

        ow_initialize(_owPioCallback);

        // Initialize timer
        {
            TCNT1 = 0;

#if TIMER_PRESCALLER == 1
            TCCR1B = _BV(CS10);
#elif TIMER_PRESCALLER == 8
            TCCR1B = _BV(CS11) | _BV(CS10);
#else
    #error "Prescaller value is not supported"
#endif
        }
    }

    ubus_hub_setup(
        &ubusHub,
        _dataBuffer,
        DATA_BUFFER_SIZE,
        _ubusRequestCallback,
        _ubusResponseCallback,
        NULL
    );

    {
        uint16_t idleCounter = 0;

        while (1) {
            if (! uart_poll()) {
                if (++idleCounter == 60000) {
                    ubus_hub_reset(&ubusHub);
                }

            } else {
                idleCounter = 0;

                ubus_hub_putByte(&ubusHub, uart_get());
            }
        }
    }
}
