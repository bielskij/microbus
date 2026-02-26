#ifndef __OW_SLAVE_DS1820_H__
#define __OW_SLAVE_DS1820_H__

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#include "ow/slave.h"

class Ds1820 : public OwSlave {
    private:
        bool     isB;
        uint8_t  resolution;
        uint64_t sn;

    public:
        Ds1820(bool isB, uint64_t sn) {
            this->isB        = isB;
            this->sn         = sn;
            this->resolution = 9;
        }

        uint64_t getSerialNumber() const override {
            return this->sn;
        }

        uint8_t getFamilyCode() const override {
            return this->isB ? 0x28 : 0x10;
        }

        void read(uint8_t *data, size_t dataSize) override {
            spdlog::debug("ds1820: read: {}", dataSize);


        }

        void write(const uint8_t *data, size_t dataSize) override {
            spdlog::debug("ds1820: write: {}", dataSize);
        }

        void reset() override {

        }
};

#endif /* __OW_SLAVE_DS1820_H__ */