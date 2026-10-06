#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <spdlog/spdlog.h>

#include "microbus/ioctl.h"

namespace fs = std::filesystem;

static void _showUsage() {
        spdlog::info("");
        spdlog::info("Usage:");
        spdlog::info("  microbusctl -d|--device <tty_device> i2c attach <address> <driver> [resource=value ...]");
        spdlog::info("  microbusctl -d|--device <tty_device> i2c dettach <address>");
        spdlog::info("");
        spdlog::info("Special resource names:");
        spdlog::info("  interrupt-source - parent interrupt source for the interface");
        spdlog::info("");
        spdlog::info("Examples:");
        spdlog::info("  microbusctl -d /dev/ttyUSB0 i2c add 0x28 pn544");
        spdlog::info("      interrupt-source=gpio:1");
        spdlog::info("      enable=gpio:2");
        spdlog::info("      firmware=gpio:3");
}

int main(int argc, char* argv[]) {
    int ret = EXIT_SUCCESS;

    {
        int fd = -1;

        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("%v");

        do {
            std::string devicePath;

            if (argc < 2) {
                _showUsage();

                ret = EXIT_FAILURE;
                break;
            }

            int arg = 1;

            while (arg < argc) {
                const std::string option = argv[arg];

                if (option == "-d" || option == "--device") {
                    if (arg + 1 >= argc) {
                        spdlog::error("-d|--device requires an argument\n");

                        _showUsage();

                        ret = EXIT_FAILURE;
                        break;
                    }

                    devicePath = argv[++arg]; ++arg;

                    continue;

                } else if (option == "-v" || option == "--verbose") {
                    spdlog::set_level(spdlog::level::debug);
                    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

                    ++arg;
                    continue;
                }

                break;
            }

            if (ret != EXIT_SUCCESS) {
                break;
            }

            if (devicePath.empty()) {
                spdlog::error("error: --device is required");

                _showUsage();

                ret = EXIT_FAILURE;
                break;
            }

            if (! fs::exists(devicePath) || ! fs::is_character_file(devicePath)) {
                spdlog::error("error: device {} does not exists or is not a character device");

                _showUsage();

                ret = EXIT_FAILURE;
                break;
            }

            if (arg >= argc) {
                _showUsage();

                ret = EXIT_FAILURE;
                break;
            }

            fd = open(devicePath.c_str(), O_RDWR);
            if (fd < 0) {
                spdlog::error("error: can't open device '{}'", devicePath);
                _showUsage();

                ret = EXIT_FAILURE;
                break;
            }

            {
                MicrobusInformation info;

                if (ioctl(fd, MICROBUS_IOC_GET_INFORMATION, &info) < 0) {
                    spdlog::error("Failed to retrieve Microbus ABI version");

                    ret = EXIT_FAILURE;
                    break;
                }

                if (
                    info.versionMajor != MICROBUS_ABI_VERSION_MAJOR ||
                    info.versionMinor != MICROBUS_ABI_VERSION_MINOR
                ) {
                    spdlog::error("error: Unsupported Microbus ABI version {}.{}", info.versionMajor, info.versionMinor);

                    ret = EXIT_FAILURE;
                    break;
                }

                spdlog::info(
                    "Have microbus interface with ABI version {}.{}, features{}", 
                    info.versionMajor, info.versionMinor,
                    std::string((info.features & MICROBUS_FEATURE_FLAG_I2C)   ? " I2C"  : "") + 
                    std::string((info.features & MICROBUS_FEATURE_FLAG_OW)    ? " OW"   : "") + 
                    std::string((info.features & MICROBUS_FEATURE_FLAG_GPIO)  ? " GPIO" : "")
                );

                std::string device = argv[arg++];

                if (device == "i2c") {
                    std::string command;

                    if (arg >= argc) {
                        _showUsage();

                        ret = EXIT_FAILURE;
                        break;
                    }

                    command = argv[arg++];

                    if (command == "attach") {
                        std::string driver;
                        uint16_t    address;

                        if (arg + 1 >= argc) {
                            spdlog::error("The command '{}' requires at least driver name and device address", command);

                            _showUsage();

                            ret = EXIT_FAILURE;
                            break;
                        }

                        address = std::stoul(argv[arg++], 0, 0);
                        driver  = argv[arg++];

                        spdlog::info("Attaching a slave device of address {} (0x{:02X}) with driver '{}'", address, address, driver);

                        {
                            MicrobusI2cAttachParameters params;

                            memset(&params, 0, sizeof(params));

                            params.address = address;

                            strncpy(params.driver, driver.c_str(), MICROBUS_NAME_LEN - 1);

                            while (arg < argc) {
                                std::string res = argv[arg++];

                                const auto equal = res.find('=');
                                if (equal == std::string::npos) {
                                    ret = EXIT_FAILURE;
                                    break;
                                }

                                auto &ioRes = params.resources[params.resourceCount++];

                                strncpy(ioRes.name, res.c_str(), std::min(equal, (size_t) MICROBUS_NAME_LEN));

                                {
                                    std::string resourceParams = res.substr(equal + 1);

                                    auto delmiter = resourceParams.find(':');

                                    if (resourceParams.substr(0, delmiter) == "gpio") {
                                        ioRes.interface = MICROBUS_INTERFACE_GPIO;
                                        ioRes.index     = std::stoi(resourceParams.substr(delmiter + 1));

                                    } else {
                                        spdlog::error("Unknown interface!");
                                    }
                                }

                                spdlog::info("res: '{}', interface: {}, index: {}", ioRes.name, ioRes.interface, ioRes.index);
                            }

                            if (ret != EXIT_SUCCESS) {
                                break;
                            }

                            if (ioctl(fd, MICROBUS_IOC_I2C_ATTACH, &params) != 0) {
                                spdlog::error("Unable to attach new {} device.", device);

                            } else {
                                spdlog::info("New {} device has been successfully attached", device);
                            }
                        }

                    } else if (command == "detach") {
                        MicrobusI2cDetachParameters params;

                        memset(&params, 0, sizeof(params));

                        {
                            params.address = std::stoul(argv[arg++], 0, 0);
                        }

                        if (ioctl(fd, MICROBUS_IOC_I2C_DETACH, &params)) {
                            spdlog::error("Unable to detach {} device at address 0x{:02X}.", device, params.address);
                        }

                    } else {
                        spdlog::error("Provided not supported command '{}' for device of type: '{}'", command, device);

                        _showUsage();

                        ret = EXIT_FAILURE;
                        break;
                    }

                } else {
                    spdlog::error("Provided not supported device type '{}'", device);

                    _showUsage();

                    ret = EXIT_FAILURE;
                    break;
                }
            }

        } while (0);

        if (fd >= 0) {
            close(fd);
        }
    }

    return ret;
}