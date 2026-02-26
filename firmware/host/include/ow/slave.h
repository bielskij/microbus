#ifndef __OW_SLAVE_H__
#define __OW_SLAVE_H__

#include <array>
#include <vector>
#include <cstdint>

#define CRC_POLY_OW 0x8C
class OwSlave {
    public:
        virtual ~OwSlave() = default;

        virtual uint64_t getSerialNumber() const = 0;

        virtual uint8_t getFamilyCode() const = 0;

        virtual void read(uint8_t *data, size_t dataSize) = 0;

        virtual void write(const uint8_t *data, size_t dataSize) = 0;

        virtual void reset() = 0;
};

#endif /* __OW_SLAVE_H__ */