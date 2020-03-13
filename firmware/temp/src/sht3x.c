#include "sht3x.h"

#include "mgos.h"
#include "mgos_i2c.h"

#define SHT31_ADDR_A 0x44
#define SHT31_ADDR_B 0x45

static uint8_t calc_crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0xff;
  size_t i, j;
  for (i = 0; i < len; i++) {
    crc ^= data[i];
    for (j = 0; j < 8; j++) {
      if ((crc & 0x80) != 0) {
        crc = (uint8_t)((crc << 1) ^ 0x31);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

static bool sht3x_probe_addr(int addr) {
  struct mgos_i2c *i2c = mgos_i2c_get_bus(0);
  if (i2c == NULL) return false;
  // Try reading the status register and verifying CRC.
  const uint8_t cmd[2] = {0xf3, 0x2d};
  if (!mgos_i2c_write(i2c, addr, cmd, 2, false /* stop */)) {
    return false;
  }
  uint8_t resp[3] = {0};
  if (!mgos_i2c_read(i2c, addr, resp, 3, true /* stop */)) {
    return false;
  }
  if (calc_crc8(resp, 2) != resp[2]) {
    LOG(LL_INFO, ("%#02x: Invalid CRC (%02x %02x %02x)", addr, resp[0], resp[1], resp[2]));
    return false;
  }
  LOG(LL_INFO, ("%#02x: Sensor status: 0x%02x%02x", addr, resp[0], resp[1]));
  // Clear status.
  const uint8_t cmd2[2] = {0x30, 0x41};
  if (!mgos_i2c_write(i2c, addr, cmd2, 2, true /* stop */)) {
    return false;
  }
  // Need to wait some time after clearing status.
  mgos_msleep(5);
  return true;
}

bool sht3x_probe(int *addr) {
  if (sht3x_probe_addr(SHT31_ADDR_A)) {
    *addr = SHT31_ADDR_A;
    return true;
  }
  if (sht3x_probe_addr(SHT31_ADDR_B)) {
    *addr = SHT31_ADDR_B;
    return true;
  }
  return false;
}

void sht3x_read(int addr, float *temp, float *rh) {
  *temp = SHT31_INVALID_VALUE;
  *rh = SHT31_INVALID_VALUE;
  struct mgos_i2c *i2c = mgos_i2c_get_bus(0);
  // High repeatability, no clock stretching.
  const uint8_t cmd[2] = {0x24, 0x00};
  int64_t start = mgos_uptime_micros();
  if (!mgos_i2c_write(i2c, addr, cmd, 2, true /* stop */)) {
    LOG(LL_ERROR, ("I2C write failed"));
    return;
  }
  uint8_t resp[6] = {0};
  mgos_msleep(12);  // Takes about 12 ms to perform the measurement.
  while (!mgos_i2c_read(i2c, addr, resp, 6, true /* stop */)) {
    if (mgos_uptime_micros() - start > 50000) {  // 50 ms
      LOG(LL_ERROR, ("Read timeout"));
      return;
    }
  }
  // Temperature
  if (calc_crc8(resp, 2) == resp[2]) {
    uint16_t st = (((uint16_t) resp[0]) << 8) | resp[1];
    *temp = -45.0 + 175.0 * ((float) st) / 65535.0;
  } else {
    LOG(LL_INFO, ("Invalid CRC (%02x %02x %02x)", resp[0], resp[1], resp[2]));
  }
  // RH
  if (calc_crc8(resp + 3, 2) == resp[5]) {
    uint16_t srh = (((uint16_t) resp[3]) << 8) | resp[4];
    *rh = 100.0 * ((float) srh) / 65535.0;
  } else {
    LOG(LL_INFO, ("Invalid CRC (%02x %02x %02x)", resp[3], resp[4], resp[5]));
  }
}
