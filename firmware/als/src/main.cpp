#include <sstream>
#include <string>
#include <vector>

#include "mgos.hpp"
#include "mgos_json_utils.hpp"
#include "mgos_rpc.h"

std::vector<int> s_pins;
static double s_last_poll = 0.0;

static int GetZone(int pin) {
  int zone = 1;
  for (int pi : s_pins) {
    if (pi == pin) return zone;
    zone++;
  }
  return -1;
}

static void AppendData(std::string &data, int pin, bool value) {
  int zone = GetZone(pin);
  int sid = mgos_sys_config_get_sensor_id();
  int subid = mgos_sys_config_get_sensor_subid_base() + zone;
  mgos::JSONAppendStringf(&data, "{sid: %d, subid: %d, name: \"%s %d\", v: %d}",
                          sid, subid, "Zone", zone, value);
}

static void ReportData(const std::string &data) {
  int sid = mgos_sys_config_get_sensor_id();
  const char *hub_addr = mgos_sys_config_get_hub_address();
  if (sid < 0 || hub_addr == nullptr) return;
  double now = mg_time();
  struct mg_rpc_call_opts opts = {};
  opts.dst = mg_mk_str(hub_addr);
  mg_rpc_callf(mgos_rpc_get_global(), mg_mk_str("Sensor.DataMulti"), nullptr,
               nullptr, &opts, "{ts: %.3f, data: [%s]}", now, data.c_str());
}

static bool ReadPin(int pin, bool is_int) {
  bool value = mgos_gpio_read(pin);
  mgos_gpio_toggle(LED_PIN);
  if (mgos_sys_config_get_invert()) {
    value = !value;
  }
  LOG(LL_INFO,
      ("%d: %d (%s)", GetZone(pin), value, (is_int ? "change" : "poll")));
  mgos_gpio_toggle(LED_PIN);
  return value;
}

static void ReadPins() {
  if (mg_time() - s_last_poll < mgos_sys_config_get_interval()) {
    return;
  }
  LOG(LL_INFO, ("Reading pins"));
  std::string data;
  for (size_t i = 0; i < s_pins.size(); i++) {
    int pin = s_pins[i];
    bool value = ReadPin(pin, false /* is_int */);
    if (i > 0) data.append(", ");
    AppendData(data, pin, value);
  }
  ReportData(data);
}

static void PinInt(int pin, void *) {
  bool value = ReadPin(pin, true /* is_int */);
  std::string data;
  AppendData(data, pin, value);
  ReportData(data);
}

static void ReadPinsEv(int, void *, void *) {
  if (mg_time() < 10000000) return;
  ReadPins();
}

extern "C" enum mgos_app_init_result mgos_app_init() {
  std::string item;
  std::istringstream iss(CS_STRINGIFY_MACRO(ZONE_PINS));
  while (std::getline(iss, item, ',')) {
    std::istringstream its(item);
    int gpio = -1;
    its >> gpio;
    s_pins.push_back(gpio);
    mgos_gpio_setup_input(gpio, MGOS_GPIO_PULL_UP);
    mgos_gpio_set_button_handler(gpio, MGOS_GPIO_PULL_UP,
                                 MGOS_GPIO_INT_EDGE_ANY, 20, PinInt, nullptr);
  }
  mgos_set_timer(
      mgos_sys_config_get_interval() * 1000, MGOS_TIMER_REPEAT,
      [](void *) { ReadPins(); }, nullptr);
#if 0
  mgos_gpio_setup_input(BTN_PIN, MGOS_GPIO_PULL_UP);
  mgos_gpio_set_button_handler(
      BTN_PIN, MGOS_GPIO_PULL_UP, MGOS_GPIO_INT_EDGE_NEG, 20,
      [](int, void *) { ReadPins(); }, nullptr);
#endif
  mgos_event_add_handler(MGOS_NET_EV_IP_ACQUIRED, ReadPinsEv, nullptr);
  mgos_event_add_handler(MGOS_EVENT_TIME_CHANGED, ReadPinsEv, nullptr);
  return MGOS_APP_INIT_SUCCESS;
}
