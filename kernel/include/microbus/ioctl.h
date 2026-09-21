#ifndef __MICROBUS_IOCTL_H__
#define __MICROBUS_IOCTL_H__

#include <linux/ioctl.h>
#include <linux/types.h>

#define MICROBUS_IOC_MAGIC 'M'

#define MICROBUS_MAX_RESOURCES 16
#define MICROBUS_NAME_LEN      64

#define MICROBUS_RESOURCE_INTERRUPT_SOURCE "interrupt-source"

enum MicrobusInterface {
    I2C,
    OW
};

struct MicrobusSearchStep {
    __u8  type;
    __u64 rn;
    __u8  descBit;
    __u8  lastZero;
    __u8  wasLast;
    __u8  found;
    __u8  reserved[3];
};

struct MicrobusResource {
    __u32 interface;
    char  name[MICROBUS_NAME_LEN];
};

struct MicrobusI2cAttachParameters {
    char  driver[MICROBUS_NAME_LEN];
    __u16 address;

    MicrobusResource resources[MICROBUS_MAX_RESOURCES];
    __u16            resourceCount;
};

#define MICROBUS_IOC_RESET        _IOR (MICROBUS_IOC_MAGIC, 0, __u8)

#define MICROBUS_IOC_SEARCH_START _IOWR(MICROBUS_IOC_MAGIC, 1, struct MicrobusSearchStep)

#define MICROBUS_IOC_SEARCH_STEP  _IOWR(MICROBUS_IOC_MAGIC, 2, struct MicrobusSearchStep)

#define MICROBUS_IOC_I2C_ATTACH   _IOW (MICROBUS_IOC_MAGIC, 3, struct MicrobusI2cAttachParameters)

#endif /* __MICROBUS_IOCTL_H__ */