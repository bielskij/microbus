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

static uint16_t _owDelays[] = {
     6, // OW_DELAY_TX_1_HI
    64, // OW_DELAY_TX_1_LO
    60, // OW_DELAY_TX_0_HI
    10, // OW_DELAY_TX_0_LO

     6, // OW_DELAY_RX_LO
     9, // OW_DELAY_RX_HI
    55, // OW_DELAY_RX_END

   490, // OW_DELAY_PRESENCE_LO
    70, // OW_DELAY_PRESENCE_HI
   410  // OW_DELAY_PRESENCE_END
};

static inline void _delayUs(uint16_t us) {
    uint32_t cycles = (uint32_t) us * (F_CPU / 1000000UL);
    uint16_t loops  = cycles / 4;

    __asm__ volatile (
        "1: sbiw %0, 1" "\n\t"
        "brne 1b"
        : "=w" (loops)
        : "0" (loops)
    );
}

static void _owDelayCallback(OwDelay delay) {
    _delayUs(_owDelays[delay]);
}

static void _owPioDirCallback(bool in, bool hi) {
    if (hi) {
        PIO_SET_HIGH(OW_PIO_BANK, OW_PIO_PIN);

    } else {
        PIO_SET_LOW(OW_PIO_BANK, OW_PIO_PIN);
    }

    if (in) {
        PIO_SET_INPUT(OW_PIO_BANK, OW_PIO_PIN);

    } else {
        PIO_SET_OUTPUT(OW_PIO_BANK, OW_PIO_PIN);
    }
}

static bool _owPioValueCallback() {
    return PIO_IS_HIGH(OW_PIO_BANK, OW_PIO_PIN);
}

int main(int argc, char *argv[]) {
    UbusHub ubusHub;

    uart_initialize();
    i2c_initialize();

    {
        PIO_SET_INPUT(OW_PIO_BANK, OW_PIO_PIN);
        PIO_SET_HIGH(OW_PIO_BANK, OW_PIO_PIN);

        ow_initialize(
            _owDelayCallback,
            _owPioDirCallback,
            _owPioValueCallback
        );
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
