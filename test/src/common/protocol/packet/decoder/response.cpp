// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#include <vector>
#include <functional>

#include <gtest/gtest.h>

#include "common/protocol.h"
#include "common/protocol/command.h"
#include "common/protocol/packet/decoder.h"
#include "common/protocol/response.h"

struct ResponseDecoderTestData {
    uint8_t                            cmd;
    std::function<void(ProtoRes &req)> prepareRes;
    std::function<void(ProtoRes &req)> fillRes;
    std::function<void(ProtoRes &req)> validateRes;
};

class ResponseDecoderTestWithParameter : public testing::TestWithParam<ResponseDecoderTestData> {
};

static std::vector<uint8_t> genCmd(uint8_t cmd, uint8_t id, std::function<void(ProtoRes &req)> prepareRes, std::function<void(ProtoRes &req)> fillRes) {
    std::vector<uint8_t> ret;

    uint8_t  packetBuffer[512];
    ProtoPkt packet;

    proto_pkt_init(&packet, packetBuffer, sizeof(packetBuffer), cmd, id);

    {
        ProtoRes response;

        proto_res_init(&response, packet.payload, packet.payloadSize, packet.code);

        if (prepareRes) {
            prepareRes(response);
        }

        proto_res_assign(&response, packet.payload, packet.payloadSize);

        if (fillRes) {
            fillRes(response);
        }

        packet.payloadUsed = proto_res_encode(&response, packet.payload, packet.payloadSize);
    }

    if (proto_pkt_encode(&packet)) {
        ret.insert(ret.end(), packet.header,  packet.header  + packet.headerUsed);
        ret.insert(ret.end(), packet.payload, packet.payload + packet.payloadUsed);
        ret.insert(ret.end(), packet.footer,  packet.footer  + packet.footerUsed);
    }

    return ret;
}

TEST_P(ResponseDecoderTestWithParameter, common_protocol) {
    auto data = genCmd(
        GetParam().cmd, 1,
        [](ProtoRes &res) {
            auto &f = GetParam().prepareRes;
            if (f) {
                f(res);
            }
        },
        [](ProtoRes &res) {
            auto &f = GetParam().fillRes;
            if (f) {
                f(res);
            }
        }
    );

    ASSERT_FALSE(data.empty());

    {
        ProtoPktDec decoder;
        ProtoPkt    packet;
        uint16_t    packetDataSize = data.size();
        uint8_t     packetData[packetDataSize];

        proto_pkt_init(&packet, packetData, packetDataSize, 0, 0);

        proto_pkt_dec_reset(&decoder, &packet);

        for (auto i = 0; i < data.size() - 1; i++) {
            ASSERT_EQ(proto_pkt_dec_putByte(&decoder, data[i], &packet), PROTO_PKT_DES_RET_IDLE);
        }

        auto res = proto_pkt_dec_putByte(&decoder, *data.rbegin(), &packet);

        ASSERT_NE(res, PROTO_PKT_DES_RET_IDLE);

        ASSERT_EQ(PROTO_PKT_DES_RET_GET_ERROR_CODE(res), PROTO_NO_ERROR);

        ASSERT_EQ(packet.code, GetParam().cmd);
        ASSERT_EQ(packet.id,   1);

        {
            ProtoRes response;

            proto_res_init(&response, packet.payload, packet.payloadUsed, packet.code);

            ASSERT_TRUE(proto_res_decode(&response, packet.payload, packet.payloadUsed));

            if (GetParam().validateRes) {
                GetParam().validateRes(response);
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(common_protocol, ResponseDecoderTestWithParameter, testing::Values(
    // Get info
    ResponseDecoderTestData {
        PROTO_CMD_GET_INFO,
        [](ProtoRes &res) {
            auto &i = res.response.getInfo;

            i.features      = PROTO_FEATURE_I2C;
            i.packetSize    = 512;
            i.version.major = 4;
            i.version.minor = 5;

            i.spiMode0 = true;
        },
        [](ProtoRes &res) {

        },
        [](ProtoRes &res) {
            auto &i = res.response.getInfo;

            ASSERT_EQ(i.features,      PROTO_FEATURE_I2C);
            ASSERT_EQ(i.packetSize,    512);
            ASSERT_EQ(i.version.major, 4);
            ASSERT_EQ(i.version.minor, 5);

            ASSERT_EQ(i.spiMode0, false);
            ASSERT_EQ(i.spiMode1, false);
            ASSERT_EQ(i.spiMode2, false);
            ASSERT_EQ(i.spiMode3, false);
        }
    },

    ResponseDecoderTestData {
        PROTO_CMD_GET_INFO,
        [](ProtoRes &res) {
            auto &i = res.response.getInfo;

            i.features      = PROTO_FEATURE_SPI;
            i.packetSize    = 512;
            i.version.major = 4;
            i.version.minor = 5;

            i.spiMode2 = true;
            i.spiMode3 = true;
        },
        [](ProtoRes &res) {

        },
        [](ProtoRes &res) {
            auto &i = res.response.getInfo;

            ASSERT_EQ(i.features,      PROTO_FEATURE_SPI);
            ASSERT_EQ(i.packetSize,    512);
            ASSERT_EQ(i.version.major, 4);
            ASSERT_EQ(i.version.minor, 5);

            ASSERT_EQ(i.spiMode0, false);
            ASSERT_EQ(i.spiMode1, false);
            ASSERT_EQ(i.spiMode2, true);
            ASSERT_EQ(i.spiMode3, true);
        }
    },

    // Read 64 (NAK)
    ResponseDecoderTestData {
        PROTO_CMD_I2C_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.i2cTransfer;

            t.rxBufferSize = 64;
            t.status       = PROTO_I2C_STATUS_NAK_ADDRESS;
        },
        [](ProtoRes &res) {
        },
        [](ProtoRes &res) {
            auto &t = res.response.i2cTransfer;

            ASSERT_EQ(t.rxBufferSize, 0);
            ASSERT_EQ(t.status,       PROTO_I2C_STATUS_NAK_ADDRESS);
        }
    },

    // Read 64 (OK)
    ResponseDecoderTestData {
        PROTO_CMD_I2C_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.i2cTransfer;

            t.rxBufferSize = 64;
            t.status       = PROTO_I2C_STATUS_OK;
        },
        [](ProtoRes &res) {
            auto &t = res.response.i2cTransfer;

            ASSERT_EQ(t.rxBufferSize, 64);

            for (uint16_t i = 0; i < t.rxBufferSize; i++) {
                t.rxBuffer[i] = i;
            }
        },
        [](ProtoRes &res) {
            auto &t = res.response.i2cTransfer;

            ASSERT_EQ(t.rxBufferSize, 64);
            ASSERT_EQ(t.status,       PROTO_I2C_STATUS_OK);

            for (uint16_t i = 0; i < t.rxBufferSize; i++) {
                ASSERT_EQ(t.rxBuffer[i], i);
            }
        }
    },

    // Read 256 (OK)
    ResponseDecoderTestData {
        PROTO_CMD_I2C_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.i2cTransfer;

            t.rxBufferSize = 256;
            t.status       = PROTO_I2C_STATUS_OK;
        },
        [](ProtoRes &res) {
            auto &t = res.response.i2cTransfer;

            ASSERT_EQ(t.rxBufferSize, 256);

            for (uint16_t i = 0; i < t.rxBufferSize; i++) {
                t.rxBuffer[i] = i;
            }
        },
        [](ProtoRes &res) {
            auto &t = res.response.i2cTransfer;

            ASSERT_EQ(t.rxBufferSize, 256);
            ASSERT_EQ(t.status,       PROTO_I2C_STATUS_OK);

            for (uint16_t i = 0; i < t.rxBufferSize; i++) {
                ASSERT_EQ(t.rxBuffer[i], i);
            }
        }
    },

    // OW
    ResponseDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            t.status = PROTO_OW_STATUS_NO_PRESENCE;
        },
        [](ProtoRes &res) {
        },
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            ASSERT_EQ(t.status, PROTO_OW_STATUS_NO_PRESENCE);
        }
    },

    ResponseDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            t.type   = PROTO_OW_TRANSFER_TYPE_RESET;
            t.status = PROTO_OW_STATUS_NO_PRESENCE;
        },
        [](ProtoRes &res) {
        },
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            ASSERT_EQ(t.status, PROTO_OW_STATUS_NO_PRESENCE);
        }
    },

    ResponseDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            t.type   = PROTO_OW_TRANSFER_TYPE_SEARCH_START;
            t.status = PROTO_OW_STATUS_SEARCH_STEP;

            t.data.search.romId     = 0x8877665544332211ULL;
            t.data.search.descBit   = 1;
            t.data.search.lastZero  = 2;
        },
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;
        },
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            ASSERT_EQ(t.status, PROTO_OW_STATUS_SEARCH_STEP);
            ASSERT_EQ(t.type,   PROTO_OW_TRANSFER_TYPE_SEARCH_START);

            ASSERT_EQ(t.data.search.romId,     0x8877665544332211ULL);
            ASSERT_EQ(t.data.search.descBit,   1);
            ASSERT_EQ(t.data.search.lastZero,  2);
        }
    },

    ResponseDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            t.type   = PROTO_OW_TRANSFER_TYPE_SEARCH_STEP;
            t.status = PROTO_OW_STATUS_SEARCH_STEP;

            t.data.search.romId     = 0x8877665544332211ULL;
            t.data.search.descBit   = 1;
            t.data.search.lastZero  = 2;
        },
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;
        },
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            ASSERT_EQ(t.status, PROTO_OW_STATUS_SEARCH_STEP);
            ASSERT_EQ(t.type,   PROTO_OW_TRANSFER_TYPE_SEARCH_STEP);

            ASSERT_EQ(t.data.search.romId,     0x8877665544332211ULL);
            ASSERT_EQ(t.data.search.descBit,   1);
            ASSERT_EQ(t.data.search.lastZero,  2);
        }
    },

    ResponseDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            t.type   = PROTO_OW_TRANSFER_TYPE_READ;
            t.status = PROTO_OW_STATUS_OK;
        },
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            t.data.transfer.dataSize = 256;

            for (uint16_t i = 0; i < t.data.transfer.dataSize; i++) {
                t.data.transfer.data[i] = i;
            }
        },
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            ASSERT_EQ(t.status, PROTO_OW_STATUS_OK);
            ASSERT_EQ(t.type,   PROTO_OW_TRANSFER_TYPE_READ);

            ASSERT_EQ(t.data.transfer.dataSize, 256);

            for (uint16_t i = 0; i < t.data.transfer.dataSize; i++) {
                ASSERT_EQ(t.data.transfer.data[i], i);
            }
        }
    },

    ResponseDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            t.type   = PROTO_OW_TRANSFER_TYPE_WRITE;
            t.status = PROTO_OW_STATUS_OK;
        },
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;
        },
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            ASSERT_EQ(t.status, PROTO_OW_STATUS_OK);
            ASSERT_EQ(t.type,   PROTO_OW_TRANSFER_TYPE_WRITE);
        }
    },

    ResponseDecoderTestData {
        PROTO_CMD_OW_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            t.type   = PROTO_OW_TRANSFER_TYPE_TOUCH_BIT;
            t.status = PROTO_OW_STATUS_OK;

            t.data.touchBit.value = 1;
        },
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;
        },
        [](ProtoRes &res) {
            auto &t = res.response.owTransfer;

            ASSERT_EQ(t.status, PROTO_OW_STATUS_OK);
            ASSERT_EQ(t.type,   PROTO_OW_TRANSFER_TYPE_TOUCH_BIT);

            ASSERT_EQ(t.data.touchBit.value, 1);
        }
    },

    ResponseDecoderTestData {
        PROTO_CMD_SPI_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.spiTransfer;

            t.rxBufferSize = 0;
        },
        [](ProtoRes &res) {
            auto &t = res.response.spiTransfer;
        },
        [](ProtoRes &res) {
            auto &t = res.response.spiTransfer;

            ASSERT_EQ(t.rxBufferSize, 0);
            ASSERT_EQ(t.rxBuffer,     nullptr);
        }
    },

    ResponseDecoderTestData {
        PROTO_CMD_SPI_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.spiTransfer;

            t.rxBufferSize = 16;
        },
        [](ProtoRes &res) {
            auto &t = res.response.spiTransfer;

            for (int i = 0; i < t.rxBufferSize; i++) {
                t.rxBuffer[i] = i;
            }
        },
        [](ProtoRes &res) {
            auto &t = res.response.spiTransfer;

            ASSERT_EQ(t.rxBufferSize, 16);
            ASSERT_NE(t.rxBuffer,     nullptr);

            for (int i = 0; i < t.rxBufferSize; i++) {
                ASSERT_EQ(t.rxBuffer[i], i);
            }
        }
    },

    ResponseDecoderTestData {
        PROTO_CMD_SPI_TRANSFER,
        [](ProtoRes &res) {
            auto &t = res.response.spiTransfer;

            t.rxBufferSize = 256;
        },
        [](ProtoRes &res) {
            auto &t = res.response.spiTransfer;

            for (int i = 0; i < t.rxBufferSize; i++) {
                t.rxBuffer[i] = i;
            }
        },
        [](ProtoRes &res) {
            auto &t = res.response.spiTransfer;

            ASSERT_EQ(t.rxBufferSize, 256);
            ASSERT_NE(t.rxBuffer,     nullptr);

            for (int i = 0; i < t.rxBufferSize; i++) {
                ASSERT_EQ(t.rxBuffer[i], i);
            }
        }
    }
));