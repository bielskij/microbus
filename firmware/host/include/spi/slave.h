#ifndef __SPI_SLAVE_H__
#define __SPI_SLAVE_H__

#include <array>
#include <vector>
#include <cstdint>

class SpiSlave {
    public:
        virtual ~SpiSlave() = default;

        virtual void transfer(uint8_t *buffer, size_t bufferSize, size_t skipSize);
};

#endif /* __SPI_SLAVE_H__ */