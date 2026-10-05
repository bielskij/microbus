// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <avr/interrupt.h>

#include <util/twi.h>
#include <util/delay.h>
#include <util/atomic.h>

#include "common/protocol.h"
#include "common/protocol/command.h"

#include "firmware/utils.h"
#include "firmware/ubus.h"
#include "firmware/uart.h"
#include "firmware/i2c.h"
#include "firmware/ow.h"

#define OW_PIO_BANK C
#define OW_PIO_PIN  0

#define IO_EVENT_QUEUE_SIZE 32
#define IO_EVENT_QUEUE_MASK (IO_EVENT_QUEUE_SIZE - 1)

typedef struct _GpioMap {
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    volatile uint8_t *pin;
    volatile uint8_t *pcmsk;
    uint8_t           index;
    uint8_t           falling:1;
    uint8_t           rising:1;
} GpioMap;

static uint8_t _ioEventQueue[IO_EVENT_QUEUE_SIZE] = { 0 };
static uint8_t _ioEventQueueHead = 0;
static uint8_t _ioEventQueueTail = 0;

#define DATA_BUFFER_SIZE 512
static uint8_t _dataBuffer[DATA_BUFFER_SIZE] = { 0 };

static GpioMap _gpio[] = {
    { &DDRD, &PORTD, &PIND, &PCMSK2, PD2, 1, 1 },
    { &DDRD, &PORTD, &PIND, &PCMSK2, PD3, 1, 1 },
    { &DDRD, &PORTD, &PIND, &PCMSK2, PD4, 1, 1 },
    { &DDRD, &PORTD, &PIND, &PCMSK2, PD5, 1, 1 }
};

static uint8_t _gpioState[3] = { 0 };

static void _handleGpioIsr(volatile uint8_t *pinReg, uint8_t stateIdx, uint8_t isrmask);

ISR(PCINT0_vect) {
    _handleGpioIsr(&PINB, 0, PCMSK0);
}

ISR(PCINT1_vect) {
    _handleGpioIsr(&PINC, 1, PCMSK1);
}

ISR(PCINT2_vect) {
    _handleGpioIsr(&PIND, 2, PCMSK2);
}

static void _queue_clear() {
    _ioEventQueueHead = 0;
    _ioEventQueueTail = 0;
}

static void _queue_put(uint8_t data) {
    uint8_t head = _ioEventQueueHead;
    uint8_t next = (head + 1) & IO_EVENT_QUEUE_MASK;

    if (next == _ioEventQueueTail) {
        return;
    }

    _ioEventQueue[head] = data;

    _ioEventQueueHead = next;
}

static bool _queue_get(uint8_t *data) {
    uint8_t tail = _ioEventQueueTail;

    if (tail == _ioEventQueueHead) {
        return false;
    }

    *data = _ioEventQueue[tail];

    tail = (tail + 1) & IO_EVENT_QUEUE_MASK;

    return true;
}

static void _handleGpioIsr(volatile uint8_t *pinReg, uint8_t stateIdx, uint8_t isrmask) {
    uint8_t current = *pinReg;
    uint8_t changed = (_gpioState[stateIdx] ^ current) & isrmask;

    if (changed) {
        for (uint8_t i = 0; i < ARRAY_SIZE(_gpio); i++) {
            const GpioMap *gpio = &_gpio[i];

            if (gpio->pin == pinReg) {
                uint8_t gpioMask = (1 << gpio->index);

                if (changed & gpioMask) {
                    if (current & gpioMask) {
                        if (gpio->rising) {
                            _queue_put(i | 0x80);
                        }

                    } else {
                        if (gpio->falling) {
                            _queue_put(i);
                        }
                    }
                }
            }
        }
    }

    _gpioState[stateIdx] = current;
}

static void _ubusRequestCallback(ProtoReq *request, ProtoRes *response, void *callbackData) {
    switch (request->cmd) {
        case PROTO_CMD_RESET:
            {
                _queue_clear();
            }
            break;

        case PROTO_CMD_GET_INFO:
            {
                ProtoResGetInfo *info = &response->response.getInfo;

                info->features = PROTO_FEATURE_I2C | PROTO_FEATURE_OW | PROTO_FEATURE_GPIO;
                
                info->gpio.count = ARRAY_SIZE(_gpio);
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
                                        req->data.search.type,
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
                                        req->data.search.type,
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

                    case PROTO_OW_TRANSFER_TYPE_READ:
                        {
                            res->data.transfer.dataSize = req->data.transfer.dataSize;

                            ow_read(res->data.transfer.data, res->data.transfer.dataSize);
                        }
                        break;

                    case PROTO_OW_TRANSFER_TYPE_WRITE:
                        {
                            ow_write(req->data.transfer.data, req->data.transfer.dataSize);
                        }
                        break;

                    case PROTO_OW_TRANSFER_TYPE_TOUCH_BIT:
                        {
                            res->data.touchBit.value = ow_read_bit();
                        }
                        break;
                }
            }
            break;

        case PROTO_CMD_GPIO_CONTROL:
            {
                ProtoReqGpioControl *req = &request->request.gpioControl;
                ProtoResGpioControl *res = &response->response.gpioControl;

                GpioMap *gpio = &_gpio[req->index];

                uint8_t mask = _BV(gpio->index);

                switch (req->type) {
                    case PROTO_GPIO_CONTROL_TYPE_GET_VALUE:
                        {
                            res->data.getValue.hi = (*gpio->pin & mask) != 0;
                        }
                        break;

                    case PROTO_GPIO_CONTROL_TYPE_SET_VALUE:
                        {
                            if (*gpio->ddr & mask) {
                                if (req->data.setValue.hi) {
                                    *gpio->port |= mask;

                                } else {
                                    *gpio->port &= ~mask;
                                }
                            }
                        }
                        break;

                    case PROTO_GPIO_CONTROL_TYPE_IRQ_MASK:
                        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                            *gpio->pcmsk &= ~_BV(gpio->index);
                        }
                        break;

                    case PROTO_GPIO_CONTROL_TYPE_IRQ_UNMASK:
                        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                            // Refresh state
                            _gpioState[req->index] = *gpio->pin;

                            *gpio->pcmsk |= _BV(gpio->index);
                        }
                        break;

                    case PROTO_GPIO_CONTROL_TYPE_SET_DIRECTION:
                        {
                            if (req->data.setDirection.out) {
                                if (req->data.setDirection.hi) {
                                    *gpio->port |= mask;

                                } else {
                                    *gpio->port &= ~mask;
                                }

                                *gpio->ddr |= mask;

                            } else {
                                *gpio->ddr  &= ~mask;
                                *gpio->port &= ~mask;
                            }
                        }
                        break;

                    case PROTO_GPIO_CONTROL_TYPE_SET_IRQ_TYPE:
                        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                            gpio->rising  = req->data.setIrqType.rising;
                            gpio->falling = req->data.setIrqType.falling;
                        }
                        break;
                }
            }

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

static void _eventCallback(ProtoReqEventReport *event, void *callbackData) {
    uint8_t *data = (uint8_t *)callbackData;

    event->type = PROTO_EVENT_REPORT_TYPE_GPIO_IRQ;

    event->data.gpioIrq.index  = (*data) & 0x0f;
    event->data.gpioIrq.rising = (*data) >> 7;
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
            TCCR1B = _BV(CS11);
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

    sei();
    {
        uint16_t idleCounter = 0;
        uint8_t  ioEvent;
        bool     ioEventReady;

        while (1) {
            if (! uart_poll()) {
                if (++idleCounter == 60000) {
                    ubus_hub_reset(&ubusHub);
                }

            } else {
                idleCounter = 0;

                ubus_hub_putByte(&ubusHub, uart_get());
            }

            {
                ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                    ioEventReady = _queue_get(&ioEvent);
                }

                if (ioEventReady) {
                    ubus_hub_reportEvent(&ubusHub, _eventCallback, &ioEvent);
                }
            }
        }
    }
}
