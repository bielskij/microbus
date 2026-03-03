#include <string>
#include <cerrno>

#include <sys/ioctl.h>
#include <spdlog/spdlog.h>

#include "microbus/ioctl.h"
#include "common/crc8.h"

static bool sendReset(int fd) {
    __u8 presence = 0;

    if (ioctl(fd, MICROBUS_IOC_RESET, &presence) != 0) {
        spdlog::error("ioctl(RESET) failed on fd {}: {}", fd, strerror(errno));

    } else if (presence == 0) {
        return true;

    } else {
        spdlog::info("OW slave not present on fd {}, presence code: {}", fd, presence);
    }

    return false;
}

static bool sendMatchRom(int fd, uint64_t rn) {
    uint8_t data[9];

    data[0] = 0x55;
    data[1] = ((rn >>  0) & 0xff);
    data[2] = ((rn >>  8) & 0xff);
    data[3] = ((rn >> 16) & 0xff);
    data[4] = ((rn >> 24) & 0xff);
    data[5] = ((rn >> 32) & 0xff);
    data[6] = ((rn >> 40) & 0xff);
    data[7] = ((rn >> 48) & 0xff);
    data[8] = ((rn >> 56) & 0xff);

    if (write(fd, data, sizeof(data)) != sizeof(data)) {
        spdlog::error("Write to OW slave failed on fd {}: {}", fd, strerror(errno));

        return false;
    }

    return true;
}

int main(int argc, char *argv[]) {
    int ret = 0;

    spdlog::set_level(spdlog::level::debug);

    if (argc != 2) {
        spdlog::error("Device number not provided. Please specify the 1-Wire device number.");

        ret = 1;

    } else {

        int fd = -1;
        do {
            std::string path = "/dev/ow-";

            path += std::string(argv[1]);

            fd = open(path.c_str(), O_RDWR);
            if (fd < 0) {
                spdlog::error("Can't open 1-Wire device {} - {}", path, strerror(errno));

                ret = 1;
                break;
            }

            {
                sendReset(fd);

                std::vector<uint64_t> sensors;

                {
                    struct MicrobusSearchStep step;

                    step.type = 0xf0;

                    ioctl(fd, MICROBUS_IOC_SEARCH_START, &step);

                    while (step.found) {
                        uint8_t familyCode = step.rn & 0xff;
                        bool    unknownFamily = false;

                        switch (familyCode) {
                            case 0x10:
                                spdlog::debug("Detected DS18(S)20 ({:X})", step.rn);
                                break;

                            case 0x28:
                                spdlog::debug("Detected DS18B20   ({:X})", step.rn);
                                break;

                            case 0x22:
                                spdlog::debug("Detected DS1822    ({:X})", step.rn);
                                break;

                            case 0x3B:
                                spdlog::debug("Detected DS1825    ({:X})", step.rn);
                                break;

                            default:
                                spdlog::warn("Detected unknown sensor {:X}", step.rn);
                                unknownFamily = true;
                        }

                        if (! unknownFamily) {
                            sensors.push_back(step.rn);
                        }

                        if (step.wasLast) {
                            break;
                        }

                        ioctl(fd, MICROBUS_IOC_SEARCH_STEP, &step);
                    }
                }

                for (auto rn : sensors) {
                    sendReset(fd);
                    sendMatchRom(fd, rn);

                    {
                        uint8_t convertCmd = 0x44;

                        if (write(fd, &convertCmd, 1) != 1) {
                           spdlog::error("Write failed on fd {}: {}", fd, strerror(errno));
                           
                           continue;
                        }
                    }

                    sleep(1);

                    sendReset(fd);
                    sendMatchRom(fd, rn);

                    {
                        uint8_t readScratchpadCmd = 0xbe;

                        if (write(fd, &readScratchpadCmd, 1) != 1) {
                           spdlog::error("Write failed on fd {}: {}", fd, strerror(errno));
                           
                           continue;
                        }
                    }

                    {
                        uint8_t scratchPad[9];

                        if (read(fd, scratchPad, sizeof(scratchPad)) != sizeof(scratchPad)) {
                           spdlog::error("read failed on fd {}: {}", fd, strerror(errno));

                           continue;
                        }

                        uint8_t crcReceived   = scratchPad[8];
                        uint8_t crcCalculated = crc8_get(scratchPad, 8, 0x8C, 0);
                        if (crcReceived != crcCalculated) {
                            spdlog::error("CRC8 mismatch: received=0x{:02X}, calculated=0x{:02X}", crcReceived, crcCalculated);

                            continue;
                        }

                        spdlog::info("[{:016X}] Temperature: {:.2f}°C", rn, ((int16_t)(scratchPad[1] << 8) | scratchPad[0]) / 16.0f);
                    }
                }
            }
        } while (0);

        if (fd >= 0) {
            close(fd);
        }
    }

    return ret;
}