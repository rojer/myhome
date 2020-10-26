#include "hub_control_limit.h"

#include "mgos.h"

#include "hub.h"

Limit::Limit(int id, struct mgos_config_hub_control_limit *l)
    : id_(id), l_(l), on_(false), last_change_(0) {
  if (!IsValid()) return;
  LOG(LL_INFO, ("Limit %d/%d: en %d, min %.2lf max %.2lf, out %s", l_->sid,
                l_->subid, l_->enable, l_->min, l_->max, l_->out));
}

int Limit::id() const {
  return id_;
}

int Limit::sid() const {
  return l_->sid;
}

void Limit::set_sid(int sid) const {
  l_->sid = sid;
}

int Limit::subid() const {
  return l_->subid;
}

void Limit::set_subid(int subid) const {
  l_->subid = subid;
}

bool Limit::enable() const {
  return l_->enable;
}

void Limit::set_enable(bool enable) {
  l_->enable = enable;
}

double Limit::min() const {
  return l_->min;
}

void Limit::set_min(double min) {
  l_->min = min;
}

double Limit::max() const {
  return l_->max;
}

void Limit::set_max(double max) {
  l_->max = max;
}

std::string Limit::out() const {
  return (l_->out != nullptr ? std::string(l_->out) : "");
}

void Limit::set_out(const std::string &out) {
  mgos_conf_set_str(&l_->out, out.c_str());
}

std::set<std::string> Limit::outputs() const {
  return ParseOutputsStr(out());
}

// static
std::set<std::string> Limit::ParseOutputsStr(const std::string &out) {
  std::set<std::string> outputs;
  const char *p = out.c_str();
  struct mg_str e;
  while ((p = mg_next_comma_list_entry(p, &e, nullptr)) != nullptr) {
    outputs.insert(std::string(e.p, e.len));
  }
  return outputs;
}

bool Limit::IsValid() {
  return sid() >= 0 && subid() >= 0 && outputs().size() > 0;
}

bool Limit::Eval() {
  double age;
  bool want_on = false;
  bool enabled = l_->enable;
  struct sensor_data sd = {};
  if (!IsValid()) return false;

  if (enabled && l_->sid == 0) {
    struct tm now = {};
    struct sensor_data sd = {};
    time_t now_ts = mg_time();
    // Disable during the day when it's warm outside.
    if (hub_get_data(2, 0, &sd) && localtime_r(&now_ts, &now) != nullptr) {
      if (now.tm_hour > 10 && now.tm_hour < 18 && sd.value > 11.0) {
        LOG(LL_INFO, ("It's warm, disabling hall heating"));
        enabled = false;
      }
    }
  }

  if (!enabled) {
    want_on = false;
  } else if (!hub_get_data(l_->sid, l_->subid, &sd)) {
    LOG(LL_INFO, ("S%d/%d: no data yet", l_->sid, l_->subid));
    want_on = false;
  } else if ((age = cs_time() - sd.ts) > 300) {
    LOG(LL_INFO, ("S%d/%d: data is stale (%.3lf old)", sd.sid, sd.subid, age));
    want_on = false;
  } else if (!on_ && sd.value < l_->min) {
    LOG(LL_INFO,
        ("S%d/%d: %.3lf < %.3lf", sd.sid, sd.subid, sd.value, l_->min));
    want_on = true;
  } else if (on_ && sd.value < l_->max) {
    LOG(LL_INFO, ("S%d/%d: %s (%.3lf; min %.3lf max %.3lf)", sd.sid, sd.subid,
                  "Not ok", sd.value, l_->min, l_->max));
    want_on = true;
  } else {
    LOG(LL_INFO, ("S%d/%d: %s (%.3lf; min %.3lf max %.3lf)", sd.sid, sd.subid,
                  "Ok", sd.value, l_->min, l_->max));
    want_on = false;
  }

  if (want_on != on_) {
    on_ = want_on;
    last_change_ = cs_time();
  }

  return want_on;
}
