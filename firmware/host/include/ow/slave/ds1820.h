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

        void read(std::vector<uint8_t> &data, size_t dataSize) override {
            spdlog::debug("read: {}", dataSize);
        }

        void write(const std::vector<uint8_t> &data) override {
            spdlog::debug("write: {:a16}", spdlog::to_hex(data));
        }

        void reset() override {

        }
};

#endif /* __OW_SLAVE_DS1820_H__ */