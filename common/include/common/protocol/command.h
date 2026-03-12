// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef FIRMWARE_INCLUDE_PROTOCOL_COMMAND_H_
#define FIRMWARE_INCLUDE_PROTOCOL_COMMAND_H_

/*
 * 1) CMD_GET_INFO.
 *
 * This commands returns the following information:
 *  - protocol version
 *  - maximal payload size supported by protocol packet
 *
 * Request payload:
 *  - No payload
 *
 * Response payload:
 *  [    4b   ][    4b   ][   1/2B   ][   1B   ][   4b    ][4b]
 *  [ VER_MAJ ][ VER_MIN ][ PLD_SIZE ][FEATURES][SPI_MODES][  ]
 */
#define PROTO_CMD_GET_INFO 0x0

#define PROTO_FEATURE_I2C (1 << 0)
#define PROTO_FEATURE_OW  (1 << 1)
#define PROTO_FEATURE_SPI (1 << 2)

// SPI_MODES field in response exists only if PROTO_FEATURE_SPI flag is set in FEATURES byte
#define PROTO_FEATURE_SPI_MODE_FLAG_0 (1 << 4)
#define PROTO_FEATURE_SPI_MODE_FLAG_1 (1 << 5)
#define PROTO_FEATURE_SPI_MODE_FLAG_2 (1 << 6)
#define PROTO_FEATURE_SPI_MODE_FLAG_3 (1 << 7)
#define PROTO_FEATURE_SPI_MODE_MASK   (0xf0)
/*
 * 2) CMD_I2C_TRANSFER
 *
 * Request
 *   ADDR - (optional) slave address, exists only when PROTO_I2C_TRANSFER_FLAG_START or PROTO_I2C_TRANSFER_FLAG_REPEATED_START is set.
 *   TX_DATA - (optional) transmit buffer to send to the slave device.
 *
 * [  1B   ][  1B  ][... ]
 * [ FLAGS ][ ADDR ][DATA]
 *
 * Response
 * [  1B  ][... ]
 * [STATUS][DATA]
 */
#define PROTO_CMD_I2C_TRANSFER 0x1

#define PROTO_I2C_TRANSFER_FLAG_START          (1 << 0)
#define PROTO_I2C_TRANSFER_FLAG_REPEATED_START (1 << 1)
#define PROTO_I2C_TRANSFER_FLAG_STOP           (1 << 2)
#define PROTO_I2C_TRANSFER_FLAG_READ           (1 << 3)
#define PROTO_I2C_TRANSFER_FLAG_CONT           (1 << 4)

#define PROTO_I2C_STATUS_OK          0
#define PROTO_I2C_STATUS_NAK_ADDRESS 1
#define PROTO_I2C_STATUS_NAK_DATA    2
#define PROTO_I2C_STATUS_ARB_LOST    3 // Arbitration lost
#define PROTO_I2C_STATUS_TIMEOUT     4 // Timeout occurred in TWI internals

/*
 * 3) CMD_1W_TRANSFER
 *
 * Request
 * [ 1B ][... ]
 * [TYPE][DATA]
 *
 * * RESET
 * no extra data
 *
 * * SEARCH_START
 * no extra data
 *
 * * SEARCH_STEP
 * [   6B   ][1B][1B][1B]
 * [ ROM_ID ][SB][DB][LZ]
 *   - SB - search_bit
 *   - DB - desc_bit
 *   - LZ - last_zero
 *
 * Response
 * [  1B  ][... ]
 * [STATUS][DATA]
 */
#define PROTO_CMD_OW_TRANSFER  0x2

#define PROTO_OW_ROM_ID_SIZE 8

#define PROTO_OW_TRANSFER_TYPE_UNKNOWN      (0)
#define PROTO_OW_TRANSFER_TYPE_RESET        (1)
#define PROTO_OW_TRANSFER_TYPE_READ         (2)
#define PROTO_OW_TRANSFER_TYPE_WRITE        (3)
#define PROTO_OW_TRANSFER_TYPE_SEARCH_START (4)
#define PROTO_OW_TRANSFER_TYPE_SEARCH_STEP  (5)
#define PROTO_OW_TRANSFER_TYPE_TOUCH_BIT    (6)

// Read/write cmd results
#define PROTO_OW_STATUS_OK            0

// Reset cmd results
#define PROTO_OW_STATUS_NO_PRESENCE   1

// Scan cmd results
#define PROTO_OW_STATUS_SEARCH_STEP        2
#define PROTO_OW_STATUS_SEARCH_DONE_EMPTY  3
#define PROTO_OW_STATUS_SEARCH_DONE_FOUND  4

/*
 * 4) CMD_SPI_TRANSFER
 *
 * [  6b   ][  2b  ][  1/2B   ][    TX_SIZE     ][     1/2B     ][  1/2B   ]
 * [ FLAGS ][ MODE ][ TX_SIZE ][ TX_DATA ][ ... ][ RX_SKIP_SIZE ][ RX_SIZE ]
 */

#define PROTO_CMD_SPI_TRANSFER  0x3

#define PROTO_SPI_TRANSFER_FLAG_KEEP_CS (1 << 7)

#define PROTO_SPI_TRANSFER_MODE_0       (0x00)
#define PROTO_SPI_TRANSFER_MODE_1       (0x01)
#define PROTO_SPI_TRANSFER_MODE_2       (0x02)
#define PROTO_SPI_TRANSFER_MODE_3       (0x03)
#define PROTO_SPI_TRANSFER_MODE_MASK    (0x03)

#endif /* FIRMWARE_INCLUDE_PROTOCOL_COMMAND_H_ */
