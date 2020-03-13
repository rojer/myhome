#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHT31_INVALID_VALUE -1000.0

bool sht3x_probe(int *addr);
void sht3x_read(int addr, float *temp, float *rh);

#ifdef __cplusplus
}
#endif
