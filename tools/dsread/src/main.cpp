#include <string>
#include <cerrno>

#include <spdlog/spdlog.h>

#include <sys/ioctl.h>

#include "microbus/ioctl.h"

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
                __u8 presence;

                ioctl(fd, MICROBUS_IOC_RESET, &presence);

                if (presence == 1 || presence < 0) {
                    spdlog::info("No device has reported the presence after reset pulse - exiting");

                    break;
                }

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
                                spdlog::debug("Detected DS1820/DS18S20 ({:X})", step.rn);
                                break;

                            case 0x28:
                                spdlog::debug("Detected DS18B20 ({:X})", step.rn);
                                break;

                            case 0x22:
                                spdlog::debug("Detected DS1822 ({:X})", step.rn);
                                break;

                            case 0x3B:
                                spdlog::debug("Detected DS1825 ({:X})", step.rn);
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
                    std::vector<uint8_t> cmd;

                    {
                        __u8 presence;

                        ioctl(fd, MICROBUS_IOC_RESET, &presence);
                    }

                    cmd.push_back(0x55);
                    cmd.push_back((rn >>  0) & 0xff);
                    cmd.push_back((rn >>  8) & 0xff);
                    cmd.push_back((rn >> 16) & 0xff);
                    cmd.push_back((rn >> 24) & 0xff);
                    cmd.push_back((rn >> 32) & 0xff);
                    cmd.push_back((rn >> 40) & 0xff);
                    cmd.push_back((rn >> 48) & 0xff);
                    cmd.push_back((rn >> 56) & 0xff);

                    if (write(fd, cmd.data(), cmd.size()) != cmd.size()) {
                        spdlog::error("error {}", strerror(errno));
                    }

                    cmd.clear();
                    cmd.push_back(0x44);
                    if (write(fd, cmd.data(), cmd.size()) != cmd.size()) {
                        spdlog::error("error {}", strerror(errno));
                    }

                    sleep(1);

                    {
                        __u8 presence;

                        ioctl(fd, MICROBUS_IOC_RESET, &presence);
                    }

                    cmd.push_back(0x55);
                    cmd.push_back((rn >>  0) & 0xff);
                    cmd.push_back((rn >>  8) & 0xff);
                    cmd.push_back((rn >> 16) & 0xff);
                    cmd.push_back((rn >> 24) & 0xff);
                    cmd.push_back((rn >> 32) & 0xff);
                    cmd.push_back((rn >> 40) & 0xff);
                    cmd.push_back((rn >> 48) & 0xff);
                    cmd.push_back((rn >> 56) & 0xff);

                    if (write(fd, cmd.data(), cmd.size()) != cmd.size()) {
                        spdlog::error("error {}", strerror(errno));
                    }

                    cmd.clear();
                    cmd.push_back(0xbe);
                    if (write(fd, cmd.data(), cmd.size()) != cmd.size()) {
                        spdlog::error("error {}", strerror(errno));
                    }

                    cmd.resize(9);
                    if (read(fd, cmd.data(), cmd.size()) != cmd.size()) {
                        spdlog::error("read error {}", strerror(errno));
                    }

                    for (int i = 0; i < cmd.size(); i++) {
                        spdlog::debug("{:x}", cmd.at(i));
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