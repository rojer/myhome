#include "hub_light.h"

#include <stdbool.h>

#include "common/cs_dbg.h"

#include "mgos_gpio.h"
#include "mgos_i2c.h"
#include "mgos_sys_config.h"
#include "mgos_system.h"
#include "mgos_timers.h"

/* Light sensor: TSL2550D */
#define LIGHT_SENSOR_ADDR 0x39

static bool s_lights_on = false;
static bool s_sensor_ok = false;

static double read_sensor(struct mgos_i2c *i2c, int n) {
  uint16_t reg = (n == 0 ? 0x43 : 0x83);
  /* Turn the sensor on, if necessary. */
  if (mgos_i2c_read_reg_b(i2c, LIGHT_SENSOR_ADDR, 0x03) < 0) return -1;
  uint8_t v = mgos_i2c_read_reg_b(i2c, LIGHT_SENSOR_ADDR, reg);
  if (!(v & 0x80)) return -1;  // Bit 7 is the "valid" bit.
  int chn = (v & 0x70) >> 4;   // Bits 6-4 are the "chord number".
  int stn = (v & 0xf);         // Bits 3-0 are the "step number".
  return 16.5 * ((1 << chn) - 1) + (stn * (1 << chn));
}

static void lights_timer_cb(void *arg) {
  const struct mgos_config_hub_light *lcfg = &mgos_sys_config_get_hub()->light;
  struct mgos_i2c *i2c = mgos_i2c_get_global();
  double s0 = read_sensor(i2c, 0);
  double s1 = read_sensor(i2c, 1);
  if (s0 < 0 || s1 < 0) {
    s_sensor_ok = false;
    s0 = s1 = 0;
  } else {
    s_sensor_ok = true;
    if (s0 < lcfg->thr_lo && !s_lights_on) {
      LOG(LL_INFO, ("lights on %.2f %.2f", s0, s1));
      s_lights_on = true;
    } else if (s0 >= lcfg->thr_hi && s_lights_on) {
      LOG(LL_INFO, ("lights off %.2f %.2f", s0, s1));
      mgos_gpio_write(lcfg->relay_gpio, 0);
      s_lights_on = false;
    } else {
      LOG(LL_INFO, ("%.2f %.2f", s0, s1));
    }
  }
  mgos_gpio_write(lcfg->relay_gpio, s_lights_on);
  (void) arg;
}

bool hub_light_get_status(bool *sensor_ok, bool *lights_on) {
  if (sensor_ok != NULL) *sensor_ok = s_sensor_ok;
  if (lights_on != NULL) *lights_on = s_lights_on;
  return true;
}

bool hub_light_init(void) {
  const struct mgos_config_hub_light *lcfg = &mgos_sys_config_get_hub()->light;
  mgos_gpio_set_mode(lcfg->relay_gpio, MGOS_GPIO_MODE_OUTPUT);
  mgos_set_timer(lcfg->check_interval * 1000, MGOS_TIMER_REPEAT,
                 lights_timer_cb, NULL);
  mgos_invoke_cb(lights_timer_cb, NULL, false /* from_isr */);
  return true;
}
