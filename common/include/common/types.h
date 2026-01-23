// SPDX-License-Identifier: MIT OR GPL-2.0-only
// SPDX-FileCopyrightText: 2026 Jarosław Bielski <bielski.j@gmail.com>

#ifndef __COMMON_TYPES_H__
#define __COMMON_TYPES_H__

#ifdef __KERNEL__
    #include <linux/types.h>
#else
    #include <stdint.h>
    #include <stdbool.h>
    #include <stdlib.h>
#endif

#endif /* __COMMON_TYPES_H__ */