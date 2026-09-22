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

#include "microbus/ioctl.h"

namespace fs = std::filesystem;

static void _showUsage() {
    std::cout
        << "Usage:" << std::endl
        << std::endl
        << "  microbusctl -d|--device <tty_device> i2c attach <address> <driver> [resource=value ...]" << std::endl
        << "  microbusctl -d|--device <tty_device> i2c dettach <address>" << std::endl
        << std::endl
        << "Special resource names:" << std::endl
        << "  interrupt-source - parent interrupt source for the interface" << std::endl
        << std::endl
        << "Examples:" << std::endl
        << std::endl
        << "  microbusctl -d /dev/ttyUSB0 i2c add 0x28 pn544" << std::endl
        << "      interrupt-source=gpio:1" << std::endl
        << "      enable=gpio:2" << std::endl
        << "      firmware=gpio:3" << std::endl
        << std::endl
    ;
}

// static Interface _parseInterface(const std::string& value) {
//     if (value == "i2c") {
//         return Interface::I2C;
//     }

//     return Interface::UNKNOWN;
// }

// static unsigned int _parseNumber(const std::string& value) {
//     size_t pos = 0;

//     unsigned long result = std::stoul(value, &pos, 0);

//     if (pos != value.size()) {
//         throw std::invalid_argument("Invalid number: " + value);
//     }

//     return static_cast<unsigned int>(result);
// }

// static Resource _parseResource(const std::string& value) {
//     const auto colon = value.find(':');

//     if (colon == std::string::npos) {
//         throw std::invalid_argument("Invalid resource '" + value + "', expected <interface>:<resource>");
//     }

//     return {
//         .interface = _parseInterface(value.substr(0, colon)),
//         .resource  = _parseNumber(value.substr(colon + 1))
//     };
// }

// static Device _parseDevice(const std::string& value) {
//     std::vector<std::string> parts;

//     size_t start = 0;

//     while (true) {
//         const auto pos = value.find(':', start);

//         if (pos == std::string::npos) {
//             parts.push_back(value.substr(start));
//             break;
//         }

//         parts.push_back(value.substr(start, pos - start));
//         start = pos + 1;
//     }

//     if (parts.size() != 2) {
//         throw std::invalid_argument("Invalid device '" + value + "', expected i2c:<controller>:<address>");
//     }

//     Device result;

//     result.interface = parseInterface(parts[0]);

//     if (result.interface == Interface::OneWire) {
//         if (parts.size() != 2)
//             throw std::invalid_argument(
//                 "1wire device must be: 1wire:<controller>");

//         result.controller = parseNumber(parts[1]);
//         return result;
//     }

//     if (parts.size() != 3)
//         throw std::invalid_argument(
//             "Device must be: <interface>:<controller>:<address>");

//     result.controller = parseNumber(parts[1]);
//     result.address = parseNumber(parts[2]);

//     return result;
// }

// static void printResource(const Resource& resource)
// {
//     switch (resource.interface) {
//     case Interface::GPIO:
//         std::cout << "gpio:" << resource.resource;
//         break;

//     case Interface::SPI:
//         std::cout << "spi:" << resource.resource;
//         break;

//     case Interface::I2C:
//         std::cout << "i2c:" << resource.resource;
//         break;

//     case Interface::OneWire:
//         std::cout << "1wire:" << resource.resource;
//         break;
//     }
// }

// static int run(int argc, char* argv[])
// {
//     if (argc < 3) {
//         std::cerr
//             << "Usage:\n"
//             << "  microbusctl device add <interface>:<controller>:<address> "
//                "[property=value ...]\n\n"
//             << "Example:\n"
//             << "  microbusctl device add i2c:0:0x28 "
//                "interrupt-source=gpio:1 "
//                "gpio-enable=gpio:2 "
//                "gpio-firmware=gpio:3\n";

//         return 1;
//     }

//     if (std::string(argv[1]) != "device" ||
//         std::string(argv[2]) != "add") {
//         throw std::invalid_argument("Expected: device add");
//     }

//     Device device = parseDevice(argv[3]);

//     for (int i = 4; i < argc; ++i) {
//         const std::string argument = argv[i];

//         const auto equal = argument.find('=');

//         if (equal == std::string::npos)
//             throw std::invalid_argument(
//                 "Expected property=value, got: " + argument);

//         const std::string name = argument.substr(0, equal);
//         const std::string value = argument.substr(equal + 1);

//         if (name == "interrupt-source") {
//             device.interruptSource = parseResource(value);
//             device.hasInterruptSource = true;
//         } else if (name.rfind("gpio-", 0) == 0) {
//             device.resources.emplace_back(
//                 name.substr(5),
//                 parseResource(value));
//         } else {
//             throw std::invalid_argument(
//                 "Unknown property: " + name);
//         }
//     }

//     std::cout << "Device:\n";
//     std::cout << "  controller: " << device.controller << '\n';
//     std::cout << "  address:    0x"
//               << std::hex << device.address << std::dec << '\n';

//     if (device.hasInterruptSource) {
//         std::cout << "  interrupt-source: ";
//         printResource(device.interruptSource);
//         std::cout << '\n';
//     }

//     for (const auto& [name, resource] : device.resources) {
//         std::cout << "  " << name << ": ";
//         printResource(resource);
//         std::cout << '\n';
//     }

//     return 0;
// }

int main(int argc, char* argv[]) {
    int ret = EXIT_SUCCESS;

    {
        int fd = -1;

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
                        std::cerr << "-d|--device requires an argument" << std::endl << std::endl;

                        _showUsage();

                        ret = EXIT_FAILURE;
                        break;
                    }

                    devicePath = argv[++arg];
                    ++arg;

                    continue;
                }

                break;
            }

            if (ret != EXIT_SUCCESS) {
                break;
            }

            if (devicePath.empty()) {
                std::cerr << "error: --device is required" << std::endl << std::endl;

                _showUsage();

                ret = EXIT_FAILURE;
                break;
            }

            if (! fs::exists(devicePath) || ! fs::is_character_file(devicePath)) {
                std::cerr << "error: device " + devicePath + " does not exists or is not a character device" << std::endl;

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
                std::cerr << "error: can't open device " << devicePath << std::endl;

                _showUsage();

                ret = EXIT_FAILURE;
                break;
            }

            {
                MicrobusInformation info;

                if (ioctl(fd, MICROBUS_IOC_GET_INFORMATION, &info) < 0) {
                    std::cerr << "error: Failed to retrieve Microbus ABI version" << std::endl;

                    ret = EXIT_FAILURE;
                    break;
                }

                if (
                    info.versionMajor != MICROBUS_ABI_VERSION_MAJOR ||
                    info.versionMinor != MICROBUS_ABI_VERSION_MINOR
                ) {
                    std::cerr << "error: Unsupported Microbus ABI version "
                        << static_cast<int>(info.versionMajor) << "."
                        << static_cast<int>(info.versionMinor) << std::endl;

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