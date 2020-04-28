#include "mgos.h"

#include "mgos_rpc.h"

#define LED_GPIO 16
#define BUZZER_GPIO 12
#define PIR_GPIO 13

static int s_pir = -1;
static double s_last = 0;

static void timer_cb(void *arg) {
  int pir = (mgos_gpio_read(PIR_GPIO) == 0);
  mgos_gpio_write(LED_GPIO, pir);
  if (pir) {
    mgos_gpio_write(LED_GPIO, 1);
    mgos_gpio_blink(LED_GPIO, 0, 0);
  } else {
    mgos_gpio_blink(LED_GPIO, 100, 4900);
  }
  if (pir != s_pir) {
    LOG(LL_INFO, ("PIR: %d -> %d", s_pir, pir));
    s_pir = pir;
  } else if (s_last != 0 && (mgos_uptime() - s_last < mgos_sys_config_get_interval())) {
    return;
  }
  int sid = mgos_sys_config_get_sensor_id();
  if (sid < 0) return;
  const char *hub_addr = mgos_sys_config_get_hub_address();
  double now = mg_time();
  struct mg_rpc_call_opts opts = {.dst = mg_mk_str(hub_addr)};
  const char *sn = mgos_sys_config_get_sensor_name();
  if (sn == NULL) sn = "";
  mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.Data"), NULL, NULL,
               &opts, "{sid: %d, subid: %d, name: %Q, ts: %f, v: %d}", sid,
               0, sn, now, pir);
  s_last = mgos_uptime();
  (void) arg;
}

static void pir_int_cb(int pin, void *arg) {
  timer_cb(arg);
  (void) pin;
}

enum mgos_app_init_result mgos_app_init(void) {
  mgos_gpio_setup_input(PIR_GPIO, MGOS_GPIO_PULL_UP);
  mgos_gpio_setup_output(LED_GPIO, 0);
  mgos_gpio_setup_output(BUZZER_GPIO, 0);
  mgos_gpio_set_button_handler(PIR_GPIO, MGOS_GPIO_PULL_UP,
                               MGOS_GPIO_INT_EDGE_ANY, 20, pir_int_cb, NULL);
  mgos_set_timer(10000, MGOS_TIMER_REPEAT, timer_cb, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
