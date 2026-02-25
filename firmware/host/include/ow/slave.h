#ifndef __OW_SLAVE_H__
#define __OW_SLAVE_H__

#include <array>
#include <vector>
#include <cstdint>

class OwSlave {
    public:
        virtual ~OwSlave() = default;

        virtual uint64_t getSerialNumber() const = 0;

        virtual uint8_t getFamilyCode() const = 0;

        virtual void read(std::vector<uint8_t> &data, size_t dataSize) = 0;

        virtual void write(const std::vector<uint8_t> &data) = 0;

        virtual void reset() = 0;
};

#endif /* __OW_SLAVE_H__ */