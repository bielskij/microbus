# Kernel Module

The kernel module implements a bridge between Linux subsystems such as I²C or SPI and a target device. Currently, only a UART interface is supported for communication with the target hardware. The module is implemented by registering a custom TTY line discipline of type ``N_DEVELOPMENT``.

At the moment, the module is supported only on Linux kernels from the ``6.8`` branch.

## Compilation

### Envronment preparation

To compile the module on a Debian-based system (or inside a build Docker container), the following packages must be installed:

```bash
apt install --no-install-recommends  make linux-headers-$(uname -r) gcc
```

To build the module, enter the ``kernel`` directory and run:

```bash
make clean modules
```
## Using the module

To load the module, execute:
```bash
sudo insmod microbus_uart.ko
```
### Module parameters

The kernel module accepts the following parameters:
  * ``debug`` - Debug level (1=ERR, 2=WARN, 3=LOG, 4=DBG, 5=TRC)

### Binding the UART Interface to microbus

* Assumptions:
  * ``UART`` - path to the UART device (eg ``/dev/ttyUSB0``)
  * ``BAUD`` - target UART baudrate

* Configure UART TTY Parameters
```bash
sudo stty -F $UART $BAUD cs8 -cstopb -parenb -crtscts -ixon -ixoff -echo -echoe -icanon -opost -isig -hup
```
* Attach the ldisc to the UART
```bash
sudo ldattach 29 $UART
```
### Disconnecting the microbus ldisc from the UART
```bash
sudo killall -q -9 ldattach
```