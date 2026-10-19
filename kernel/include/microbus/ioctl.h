#ifndef __MICROBUS_IOCTL_H__
#define __MICROBUS_IOCTL_H__

#include <linux/ioctl.h>
#include <linux/types.h>

#define MICROBUS_IOC_MAGIC 'M'

#define MICROBUS_ABI_VERSION_MAJOR 1
#define MICROBUS_ABI_VERSION_MINOR 0

#define MICROBUS_MAX_RESOURCES 8
#define MICROBUS_NAME_LEN      64

#define MICROBUS_RESOURCE_INTERRUPT_SOURCE "interrupt-source"

#define MICROBUS_FEATURE_FLAG_I2C  (1 << 0)
#define MICROBUS_FEATURE_FLAG_OW   (1 << 1)
#define MICROBUS_FEATURE_FLAG_GPIO (1 << 2)

typedef enum _MicrobusInterface {
    MICROBUS_INTERFACE_I2C,
    MICROBUS_INTERFACE_OW,
    MICROBUS_INTERFACE_GPIO
} MicrobusInterface;

typedef struct _MicrobusSearchStep {
    __u8  type;
    __u64 rn;
    __u8  descBit;
    __u8  lastZero;
    __u8  wasLast;
    __u8  found;
    __u8  reserved[3];
} MicrobusSearchStep;

typedef struct _MicrobusI2cInformation {
    __u8 reserved;
} MicrobusI2cInformation;

typedef struct _MicrobusOwInformation {
    __u8 reserved;
} MicrobusOwInformation;

typedef struct _MicrobusGpioInformation {
    __u8 pinCount;
} MicrobusGpioInformation;

typedef struct _MicrobusInformation {
    __u8 versionMajor;
    __u8 versionMinor;
    __u8 features;

    MicrobusI2cInformation  i2c;
    MicrobusOwInformation   ow;
    MicrobusGpioInformation gpio;
} MicrobusInformation;

typedef struct _MicrobusResource {
    __u8 interface;
    __u8 index;
    char name[MICROBUS_NAME_LEN];
} MicrobusResource;

typedef struct _MicrobusI2cAttachParameters {
    char  driver[MICROBUS_NAME_LEN];
    __u16 address;

    MicrobusResource resources[MICROBUS_MAX_RESOURCES];
    __u16            resourceCount;
} MicrobusI2cAttachParameters;

typedef struct _MicrobusI2cDetachParameters {
    __u16 address;
} MicrobusI2cDetachParameters;

#define MICROBUS_IOC_GET_INFORMATION _IOR(MICROBUS_IOC_MAGIC, 0, MicrobusInformation)

#define MICROBUS_IOC_RESET           _IOR (MICROBUS_IOC_MAGIC, 0, __u8)

#define MICROBUS_IOC_SEARCH_START    _IOWR(MICROBUS_IOC_MAGIC, 1, MicrobusSearchStep)

#define MICROBUS_IOC_SEARCH_STEP     _IOWR(MICROBUS_IOC_MAGIC, 2, MicrobusSearchStep)

#define MICROBUS_IOC_I2C_ATTACH      _IOW (MICROBUS_IOC_MAGIC, 3, MicrobusI2cAttachParameters)

#define MICROBUS_IOC_I2C_DETACH      _IOW (MICROBUS_IOC_MAGIC, 4, MicrobusI2cDetachParameters)

#endif /* __MICROBUS_IOCTL_H__ */