# Microbus

**Microbus** is a lightweight project created to address the need for a **universal bus bridge** for embedded development, accessible directly from a **Linux host PC**.

The core idea is simple: provide a low-cost, flexible way to access common embedded buses (such as I²C, 1-Wire, and SPI) from Linux using standard kernel device subsystems. Given the abundance of inexpensive microcontrollers, mostly ARM-based, the concept of building a universal bus bridge for just a few dollars proved both practical and promising.

The project is currently **under active development**, and not all planned features are fully implemented yet.

---

## Project Overview

Microbus consists of two main components:

- **Linux kernel module**
  - Exposes embedded buses through the Linux device model (/dev/i2c-X, /dev/spi)
  - [Compilation and Module Usage](kernel/README.md)
- **Target firmware**
  - Runs on a microcontroller
  - Bridges Linux bus operations to physical hardware
  - Communicates with the host via UART or USB

This architecture allows Linux applications to interact with embedded peripherals **as if they were native Linux devices**.

---

## Supported Protocols

| Protocol | Status |
|--------|--------|
| I²C | ✅ Supported |
| 1-Wire | 🚧 Planned |
| SPI | 🚧 Planned |

---

## Supported Hardware

### Host
- Linux-based PC (kernel module required)

### Target devices
- ✅ Arduino Nano

---

## Planned Hardware Support

- ESP32-based boards
- Raspberry Pi Pico / Pico 2
- Additional low-cost ARM and RISC-V MCUs

---

## Communication Interfaces

- UART (primary)
- USB (planned / experimental)

---

## Build and Deployment

Each firmware target and the kernel module contains **its own build and deployment instructions**.

Please refer to:
- `kernel/` – Linux kernel module build instructions
- `firmware/<target>/` - target-specific firmware build and flashing guides

---

## Example Use Cases

* Assumptions
  * System environment
    * ``I2C_DEV_NO`` - index of the I²C bus to be used (e.g. 1)

### index of the I²C bus to be used (e.g. 1)
```bash
sudo i2cdetect -y 23
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:                         -- -- -- -- -- -- -- -- 
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
40: -- -- -- -- -- -- -- -- 48 -- -- -- -- -- -- -- 
50: 50 51 -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
70: -- -- -- -- -- -- -- --
```

### EEPROM Access (AT24C04)
Access external EEPROM devices over I²C directly from Linux userspace.

```bash
sudo modprobe at24

# Register the E2PROM device on the selected I²C bus
sudo bash -c "echo 24c04 0x50 > /sys/bus/i2c/devices/i2c-${I2C_DEV_NO}/new_device"

# Dump E2PROM contents
sudo cat /sys/bus/i2c/drivers/at24/${I2C_DEV_NO}$-0050/eeprom | tee eeprom-read.bin | hexdump -C
```

### ADC Access (ADS1115)
Read analog values from remote ADCs using standard Linux interfaces.
```bash
# Load the ADC driver
sudo modprobe ti-ads1015

# Register the ADC device on the selected I²C bus
sudo bash -c "echo ads1115 0x48 > /sys/bus/i2c/devices/i2c-${I2C_DEV_NO}/new_device"

# Configure input scaling (±6.144 V range)
sudo bash -c "echo 0.187500000 > /sys/bus/iio/devices/iio\:device0/in_voltage0_scale"

# Read the raw ADC value
cat /sys/bus/iio/devices/iio\:device0/in_voltage0_raw
25473

# Convert the raw value to millivolts
echo "$(cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw) * $(cat /sys/bus/iio/devices/iio:device0/in_voltage0_scale)" | bc -l
4776.937500000
```

### RTC Clock (DS1307)
Expose external RTC devices as Linux clock sources.
```bash
sudo modprobe rtc-ds1307

sudo bash -c "echo ds1307 0x68 > /sys/bus/i2c/devices/i2c-${I2C_DEV_NO}/new_device"

sudo hwclock -r
sudo hwclock -w
```

### Light Sensor (TSL2561)
```bash
# Load the kernel driver (tsl2563 supports TSL2561)
sudo modprobe tsl2563

# Register the sensor on the selected I²C bus
sudo bash -c "echo tsl2561 0x39 > /sys/bus/i2c/devices/i2c-${I2C_DEV_NO}/new_device"

# Read illuminance value (lux)
cat /sys/bus/iio/devices/iio\:device0/in_illuminance0_input 
```

---

## Project Status

⚠️ **Work in progress**

- APIs may change
- Not all protocols are implemented
- Stability is not yet guaranteed

Contributions, testing, and feedback are welcome.

---

## License

The project uses a **mixed licensing model**:
- Linux kernel module: **GPL-2.0**
- Shared and userspace components: **MIT**, depending on the file

See individual source files for SPDX license identifiers.