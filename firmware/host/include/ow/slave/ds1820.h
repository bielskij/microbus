#ifndef __OW_SLAVE_DS1820_H__
#define __OW_SLAVE_DS1820_H__

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#include "common/crc8.h"

#include "ow/slave.h"

class Ds1820 : public OwSlave {
    private:
        enum State {
            IDLE,
            READ_SCRATCHPAD
        };

    private:
        bool     isB;
        uint8_t  resolution;
        uint64_t sn;
        float    temperature;
        uint16_t readOffset;
        State    state;

        std::array<uint8_t, 9> scratchpad;

    private:
        void generateScratchPad() {
            uint16_t temp = this->temperature * 16;
            // 18b20
            this->scratchpad[0] = temp & (((uint16_t) 0xffff) << (this->resolution - 9));
            this->scratchpad[1] = temp >> 8;
            this->scratchpad[2] = 0x50;
            this->scratchpad[3] = 0x05;
            this->scratchpad[4] = ((this->resolution - 9) << 5) | 0x1f; // conf
            this->scratchpad[5] = 0xff; // reserved
            this->scratchpad[6] = 0xff; // reserved
            this->scratchpad[7] = 0x10; // reserved
            this->scratchpad[8] = crc8_get(this->scratchpad.data(), 8, CRC_POLY_OW, 0);
        }

    public:
        Ds1820(bool isB, uint64_t sn, float temperature) {
            this->isB         = isB;
            this->sn          = sn;
            this->resolution  = 9;
            this->temperature = temperature;

            this->generateScratchPad();

            this->reset();
        }

        uint64_t getSerialNumber() const override {
            return this->sn;
        }

        uint8_t getFamilyCode() const override {
            return this->isB ? 0x28 : 0x10;
        }

        void read(uint8_t *data, size_t dataSize) override {
            spdlog::debug("ds1820: read: {}", dataSize);

            for (size_t i = 0; i < dataSize; i++) {
                switch (this->state) {
                    case State::READ_SCRATCHPAD:
                        {
                            data[i] = this->scratchpad[this->readOffset++];
                            if (this->readOffset >= this->scratchpad.size()) {
                                this->state = State::IDLE;
                            }
                        }
                        break;
                }
            }
        }

        void write(const uint8_t *data, size_t dataSize) override {
            spdlog::debug("ds1820: write: {:x}, {}, state: {}", data[0], dataSize, this->state);

            for (size_t i = 0; i < dataSize; i++) {
                auto byte = data[i];

                switch (this->state) {
                    case State::IDLE:
                        if (byte == 0xbe) {
                            this->readOffset = 0;
                            this->state      = State::READ_SCRATCHPAD;
                        }
                        break;
                }
            }
        }

        void reset() override {
            this->readOffset = 0;
            this->state      = State::IDLE;
        }
};

#endif /* __OW_SLAVE_DS1820_H__ */