#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHT31_INVALID_VALUE -1000.0

bool sht3x_probe(void);
void sht3x_read(float *temp, float *rh);

#ifdef __cplusplus
}
#endif
