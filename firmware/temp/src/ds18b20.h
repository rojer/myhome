#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHT31_INVALID_VALUE -1000.0

bool ds18b20_probe(void);
void ds18b20_read(int addr, float *temp, float *rh);

#ifdef __cplusplus
}
#endif
