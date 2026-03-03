# Arduino firmware

Arduino pin configuration:
 - **PC0** (A0): 1-Wire bus
 - **PC4**: I²C SDA
 - **PC5**: I²C SCL

## Compilation

### Envronment preparation

To compile the firmware on a Debian-based system (or inside a build Docker container), the following packages must be installed:

```bash
apt install --no-install-recommends cmake make gcc-avr avr-libc avrdude 
```

To build the firmware run:

```bash
cd firmware/arduino
mkdir build
cd build
cmake ..

make all
```
### Compilation parameters
Compilation is customizable by the following parameters:
 * ``AVR_UPLOADTOOL_PORT`` - UART port used by avrdude to upload the firmware (default: ``/dev/ttyUSB0``)
 * ``AVR_UPLOADTOOL_BAUDRATE`` - UART baud rate used by avrdude (default: ``57600``)
 * ``DEFAULT_AVR_PROGRAMMER`` - programmer hardware model (default: ``arduino``)
 * ``FW_F_CPU`` - target CPU frequency in Hz (default: ``16000000``)
 * ``DEFAULT_FW_BAUD`` - Microbus UART baud rate (default: ``500000``)

**IMPORTANT**
The current version supports Arduino boards equipped with the ATmega328 only.

## Uploading the firmware to the target hardware

To upload the firmware to the target hardware, use the following make target from the build directory:
```bash
make upload_firmware
```