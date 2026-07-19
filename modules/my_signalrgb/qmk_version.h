#pragma once

#include <stdint.h>

typedef struct {
    uint8_t major, minor, patch;
} QmkVersion;

extern const QmkVersion kQmkVersion;

#define QMK_VERSION_BYTE_1 kQmkVersion.major;
#define QMK_VERSION_BYTE_2 kQmkVersion.minor;
#define QMK_VERSION_BYTE_3 kQmkVersion.patch;
