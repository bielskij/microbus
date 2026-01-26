// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <util/twi.h>

#include "common/protocol.h"
#include "common/protocol/command.h"

#include "firmware/utils.h"
#include "firmware/ubus.h"
#include "firmware/uart.h"
#include "firmware/i2c.h"

#define DATA_BUFFER_SIZE 512

static uint8_t _dataBuffer[DATA_BUFFER_SIZE] = { 0 };

static void _ubusRequestCallback(ProtoReq *request, ProtoRes *response, void *callbackData) {
    switch (request->cmd) {
        case PROTO_CMD_GET_INFO:
            {
                ProtoResGetInfo *info = &response->response.getInfo;

                info->features = PROTO_FEATURE_I2C | PROTO_FEATURE_1W;
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

        default:
            break;
    }
}

static void _ubusResponseCallback(uint8_t *buffer, uint16_t bufferSize, void *callbackData) {
    for (uint16_t i = 0; i < bufferSize; i++) {
        uart_send(buffer[i]);
    }
}

int main(int argc, char *argv[]) {
    UbusHub ubusHub;

    uart_initialize();
    i2c_initialize();

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
