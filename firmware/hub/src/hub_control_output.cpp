#include "hub_control_output.h"

#include "mgos.h"

#include "hub.h"

Output::Output(struct mgos_config_hub_control_output *o)
    : o_(o), on_(false), last_change_(0) {
  char buf[8];
  if (!IsValid()) return;
  LOG(LL_INFO, ("Output %d: name '%s', pin %s, act %d", o_->id, o_->name,
                mgos_gpio_str(o_->pin, buf), o_->act));
  mgos_gpio_setup_output(o_->pin, !o_->act);
}

int Output::id() const {
  return o_->id;
}

std::string Output::name() const {
  if (o_->name == nullptr) return "";
  return std::string(o_->name);
}

int Output::pin() const {
  return o_->pin;
}

std::string Output::pin_name() const {
  char buf[8];
  return std::string(mgos_gpio_str(o_->pin, buf));
}

int Output::act() const {
  return o_->act;
}

bool Output::IsValid() const {
  return (o_->pin >= 0 && o_->id >= 0 && o_->name != nullptr);
}

bool Output::GetState() const {
  if (!IsValid()) {
    LOG(LL_INFO, ("%d (%s): attempted to get state of an invalid output",
                  o_->id, (o_->name != nullptr ? o_->name : "")));
    return false;
  }
  return on_;
}

bool Output::GetStateWithTimestamp(double *last_change) const {
  if (!IsValid()) {
    LOG(LL_INFO, ("%d (%s): attempted to get state of an invalid output",
                  o_->id, (o_->name != nullptr ? o_->name : "")));
    *last_change = 0;
    return false;
  }
  *last_change = last_change_;
  return GetState();
}

static const char *onoff(bool on) {
  return (on ? "on" : "off");
}

void Output::SetState(bool new_state) {
  if (!IsValid()) {
    LOG(LL_INFO, ("%d (%s): attempted to set state of an invalid output",
                  o_->id, (o_->name != nullptr ? o_->name : "")));
    return;
  }
  bool old_state = GetState();
  if (new_state == old_state) return;
  LOG(LL_INFO, ("%s (%d): %s -> %s", o_->name, o_->id, onoff(old_state),
                onoff(new_state)));
  mgos_gpio_write(o_->pin, (new_state ? o_->act : !o_->act));
  last_change_ = mg_time();
  on_ = new_state;
  Report();
}

void Output::Report() {
  if (!IsValid()) return;
  int v = (GetState() ? 1 : 0);
  report_to_server(mgos_sys_config_get_hub_ctl_sid(), id(), mg_time(), v);
}
