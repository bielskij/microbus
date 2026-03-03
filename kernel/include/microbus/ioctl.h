#ifndef __MICROBUS_IOCTL_H__
#define __MICROBUS_IOCTL_H__

#include <linux/ioctl.h>
#include <linux/types.h>

#define MICROBUS_IOC_MAGIC 'M'

struct MicrobusSearchStep {
    __u8  type;
    __u64 rn;
    __u8  descBit;
    __u8  lastZero;
    __u8  wasLast;
    __u8  found;
    __u8  reserved[3];
};

#define MICROBUS_IOC_RESET        _IOR (MICROBUS_IOC_MAGIC, 0, __u8)

#define MICROBUS_IOC_SEARCH_START _IOWR(MICROBUS_IOC_MAGIC, 1, struct MicrobusSearchStep)

#define MICROBUS_IOC_SEARCH_STEP  _IOWR(MICROBUS_IOC_MAGIC, 2, struct MicrobusSearchStep)

#endif /* __MICROBUS_IOCTL_H__ */