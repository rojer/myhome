#include "hub_light.h"

#include <stdbool.h>

#include "fw/src/mgos_gpio.h"
#include "fw/src/mgos_i2c.h"
#include "fw/src/mgos_sys_config.h"
#include "fw/src/mgos_timers.h"

/* Light sensor: TSL2550D */
#define LIGHT_SENSOR_ADDR 0x39

static bool lights_on = false;

double read_sensor(struct mgos_i2c *i2c, int n) {
  uint16_t reg = (n == 0 ? 0x43 : 0x83);
  uint8_t v = mgos_i2c_read_reg_b(i2c, LIGHT_SENSOR_ADDR, reg);
  if (!(v & 0x80)) return -1;  // Bit 7 is the "valid" bit.
  int chn = (v & 0x70) >> 4;   // Bits 6-4 are the "chord number".
  int stn = (v & 0xf);         // Bits 3-0 are the "step number".
  return 16.5 * ((1 << chn) - 1) + (stn * (1 << chn));
}

void lights_timer_cb(void *arg) {
  const struct sys_config_hub_light *lcfg = &get_cfg()->hub.light;
  struct mgos_i2c *i2c = mgos_i2c_get_global();
  /* Turn the sensor on, if necessary. */
  mgos_i2c_read_reg_b(i2c, LIGHT_SENSOR_ADDR, 0x03);
  double s0 = read_sensor(i2c, 0);
  double s1 = read_sensor(i2c, 1);
  if (s0 < 0 || s1 < 0) return;
  if (s0 < lcfg->thr_lo && !lights_on) {
    LOG(LL_INFO, ("lights on %.2f %.2f", s0, s1));
    mgos_gpio_write(lcfg->relay_gpio, 1);
    lights_on = true;
  } else if (s0 >= lcfg->thr_hi && lights_on) {
    LOG(LL_INFO, ("lights off %.2f %.2f", s0, s1));
    mgos_gpio_write(lcfg->relay_gpio, 0);
    lights_on = false;
  } else {
    LOG(LL_INFO, ("%.2f %.2f", s0, s1));
  }
  (void) arg;
}

enum mgos_app_init_result hub_light_init(void) {
  struct mgos_i2c *i2c = mgos_i2c_get_global();
  const struct sys_config_hub_light *lcfg = &get_cfg()->hub.light;
  mgos_gpio_set_mode(lcfg->relay_gpio, MGOS_GPIO_MODE_OUTPUT);
  mgos_i2c_read_reg_b(i2c, LIGHT_SENSOR_ADDR, 0x03);
  mgos_set_timer(1 * 1000, false /* repeat */, lights_timer_cb, NULL);
  mgos_set_timer(lcfg->check_interval * 1000, true /* repeat */, lights_timer_cb, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
