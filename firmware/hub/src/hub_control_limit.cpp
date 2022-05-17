#include "hub_control_limit.hpp"

#include "mgos.hpp"

#include "hub_data.hpp"

Limit::Limit(int id, struct mgos_config_hub_control_limit *l)
    : id_(id), l_(l), on_(false), last_change_(0) {
  if (!IsValid()) return;
  LOG(LL_INFO, ("Limit %s", ToString().c_str()));
}

std::string Limit::ToString() const {
  return mgos::SPrintf(
      "[%d %d/%d en %d %.2lf-%.2lf inv %d deps %s out %s; on %d]", id(), sid(),
      subid(), enable(), min(), max(), invert(), DepsStr().c_str(),
      OutStr().c_str(), on_);
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

bool Limit::invert() const {
  return l_->invert;
}

void Limit::set_invert(bool invert) {
  l_->invert = invert;
}

std::string Limit::DepsStr() const {
  return (l_->deps != nullptr ? l_->deps : "");
}

void Limit::set_deps(const std::string &deps) {
  mgos_conf_set_str(&l_->deps, deps.c_str());
}

std::string Limit::OutStr() const {
  return (l_->out != nullptr ? l_->out : "");
}

void Limit::set_out(const std::string &out) {
  mgos_conf_set_str(&l_->out, out.c_str());
}

std::set<std::string> Limit::deps() const {
  return ParseCommaStr(DepsStr());
}

std::set<std::string> Limit::outputs() const {
  return ParseCommaStr(OutStr());
}

// static
std::set<std::string> Limit::ParseCommaStr(const std::string &out) {
  std::set<std::string> outputs;
  const char *p = out.c_str();
  struct mg_str e;
  while ((p = mg_next_comma_list_entry(p, &e, nullptr)) != nullptr) {
    outputs.insert(std::string(e.p, e.len));
  }
  return outputs;
}

bool Limit::IsValid() {
  return sid() >= 0 && subid() >= 0 && min() <= max();
}

bool Limit::Eval(bool quiet) {
  double age;
  bool want_on = false;
  bool enabled = l_->enable;
  struct SensorData sd;
  if (!IsValid()) return false;

  if (!enabled) {
    want_on = false;
  } else if (!hub_get_data(l_->sid, l_->subid, &sd)) {
    if (!quiet) {
      LOG(LL_INFO, ("S%d/%d: no data yet", l_->sid, l_->subid));
    }
    want_on = false;
  } else if ((age = cs_time() - sd.ts) > 300) {
    if (!quiet) {
      LOG(LL_INFO,
          ("S%d/%d: data is stale (%.3lf old)", sd.sid, sd.subid, age));
    }
    want_on = false;
  } else if (invert()) {
    if (!on_ && sd.value > max()) {
      if (!quiet) {
        LOG(LL_INFO,
            ("S%d/%d: %.3lf > %.3lf", sd.sid, sd.subid, sd.value, max()));
      }
      want_on = true;
    } else if (on_ && sd.value > min()) {
      if (!quiet) {
        LOG(LL_INFO, ("S%d/%d: %s !(%.3lf; min %.3lf max %.3lf)", sd.sid,
                      sd.subid, "Not ok", sd.value, min(), max()));
      }
      want_on = true;
    } else {
      if (!quiet) {
        LOG(LL_INFO, ("S%d/%d: %s !(%.3lf; min %.3lf max %.3lf)", sd.sid,
                      sd.subid, "Ok", sd.value, min(), max()));
      }
      want_on = false;
    }
  } else {
    if (!on_ && sd.value < l_->min) {
      if (!quiet) {
        LOG(LL_INFO,
            ("S%d/%d: %.3lf < %.3lf", sd.sid, sd.subid, sd.value, l_->min));
      }
      want_on = true;
    } else if (on_ && sd.value < l_->max) {
      if (!quiet) {
        LOG(LL_INFO, ("S%d/%d: %s (%.3lf; min %.3lf max %.3lf)", sd.sid,
                      sd.subid, "Not ok", sd.value, l_->min, l_->max));
      }
      want_on = true;
    } else {
      if (!quiet) {
        LOG(LL_INFO, ("S%d/%d: %s (%.3lf; min %.3lf max %.3lf)", sd.sid,
                      sd.subid, "Ok", sd.value, l_->min, l_->max));
      }
      want_on = false;
    }
  }

  if (want_on != on_) {
    on_ = want_on;
    last_change_ = cs_time();
  }

  return want_on;
}
