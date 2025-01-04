#include "hub_control.hpp"

#include <math.h>
#include <time.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "mgos.hpp"
#include "mgos_crontab.h"
#include "mgos_gpio.h"
#include "mgos_rpc.h"
#include "mgos_sys_config.h"
#include "mgos_timers.h"

#include "hub_control_limit.hpp"
#include "hub_control_output.hpp"
#include "hub_data.hpp"

#define NUM_LIMITS 30
#define NUM_OUTPUTS 10

static bool s_heater_on = false;
static double s_deadline = 0;

static const char *onoff(bool on) {
  return (on ? "on" : "off");
}

static Control *s_ctl = nullptr;

Control::Control(struct mgos_config_hub_control *cfg)
    : cfg_(cfg), eval_timer_(std::bind(&Control::Eval, this, false)) {
#define CASE(n)                                         \
  case n:                                               \
    ocfg = mgos_sys_config_get_hub_control_output##n(); \
    break;
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    const struct mgos_config_hub_control_output *ocfg = nullptr;
    switch (i) {
      case 0:
        ocfg = mgos_sys_config_get_hub_control_output();
        break;

        CASE(1)
        CASE(2)
        CASE(3)
        CASE(4)
        CASE(5)
        CASE(6)
        CASE(7)
        CASE(8)
        CASE(9)
    }
    outputs_.push_back(
        new Output(const_cast<struct mgos_config_hub_control_output *>(ocfg)));
  }
#undef CASE
  for (int i = 0; i < NUM_LIMITS; i++) {
    const struct mgos_config_hub_control_limit *lcfg = nullptr;
#define CASE(n)                                        \
  case n:                                              \
    lcfg = mgos_sys_config_get_hub_control_limit##n(); \
    break;
    switch (i) {
      case 0:
        lcfg = mgos_sys_config_get_hub_control_limit();
        break;

        CASE(1)
        CASE(2)
        CASE(3)
        CASE(4)
        CASE(5)
        CASE(6)
        CASE(7)
        CASE(8)
        CASE(9)
        CASE(10)
        CASE(11)
        CASE(12)
        CASE(13)
        CASE(14)
        CASE(15)
        CASE(16)
        CASE(17)
        CASE(18)
        CASE(19)
        CASE(20)
        CASE(21)
        CASE(22)
        CASE(23)
        CASE(24)
        CASE(25)
        CASE(26)
        CASE(27)
        CASE(28)
        CASE(29)
    }
#undef CASE
    limits_.push_back(
        new Limit(i, const_cast<struct mgos_config_hub_control_limit *>(lcfg)));
  }

  eval_timer_.Reset(1000, MGOS_TIMER_REPEAT);

  if (!cfg_->enable) {
    LOG(LL_INFO, ("Control is disabled"));
  }
}

bool Control::IsEnabled() const {
  return cfg_->enable;
}

void Control::SetEnabled(bool enable, const std::string &source) {
  if (enable == cfg_->enable) return;
  LOG(LL_INFO, ("Control %s -> %s (%s)", onoff(cfg_->enable), onoff(enable),
                source.c_str()));
  cfg_->enable = enable;
  Eval(true /* force */);
}

void Control::Eval(bool force) {
  double now = cs_time();
  std::set<Output *> want_outputs_on;
  LOG(LL_DEBUG,
      ("Eval %d %f %f %f", cfg_->enable, now, s_deadline, last_eval_));
  if (cfg_->enable) {
    if (s_deadline > 0 && s_deadline < now) {
      LOG(LL_INFO, ("Deadline expired"));
      if (s_heater_on) s_heater_on = false;
      s_deadline = 0;
    }
    if (s_deadline != 0) return;  // Heater is under manual control.
    if (now - last_eval_ < cfg_->eval_interval && !force) return;
    for (Limit *l : limits_) {
      struct SensorData sd;
      const uint8_t sensor_type = (l->sid() >> 24);
      // For Xavax sensors, update thresholds from target temperature.
      if (sensor_type == 1 && hub_get_data(l->sid(), 1, &sd)) {
        double want_min = sd.value - 0.5, want_max = sd.value + 0.5;
        if (l->min() != want_min || l->max() != want_max) {
          LOG(LL_INFO, ("%d: SID %d: Updating thresholds to match TT %.1f",
                        l->id(), l->sid(), sd.value));
          l->set_min(want_min);
          l->set_max(want_max);
          mgos_sys_config_save(&mgos_sys_config, false /* try_once */, nullptr);
        }
      }
      // Same for BluTRV
      if (sensor_type == 4 && hub_get_data(l->sid(), 0x4500, &sd)) {
        double want_min = sd.value - 0.5, want_max = sd.value + 0.5;
        if (l->min() != want_min || l->max() != want_max) {
          LOG(LL_INFO, ("%d: SID %d: Updating thresholds to match TT %.1f",
                        l->id(), l->sid(), sd.value));
          l->set_min(want_min);
          l->set_max(want_max);
          mgos_sys_config_save(&mgos_sys_config, false /* try_once */, nullptr);
        }
      }
      bool want_on = l->Eval();
      if (want_on) {
        for (const std::string &id : l->deps()) {
          Limit *dl = GetLimitByID(id);
          if (dl == nullptr) {
            LOG(LL_ERROR, ("Invalid dependency %s", id.c_str()));
            continue;
          }
          if (dl->Eval(true /* quiet */)) {
            LOG(LL_INFO,
                ("%d: Inhibited by %s", l->id(), dl->ToString().c_str()));
            want_on = false;
            break;
          }
        }
      }
      if (want_on) {
        for (const std::string &name_or_id : l->outputs()) {
          Output *o = GetOutputByNameOrID(name_or_id);
          if (o != nullptr) {
            std::string n = o->name();
            LOG(LL_DEBUG,
                ("Want on: %d/%d -> %s", l->sid(), l->subid(), n.c_str()));
            want_outputs_on.insert(o);
          } else {
            LOG(LL_ERROR, ("Failed to find output %s (limit %d/%d)",
                           name_or_id.c_str(), l->sid(), l->subid()));
          }
        }
      }
      if (l->IsValid() && l->enable()) {
        report_to_server(mgos_sys_config_get_hub_lim_sid(), l->id(), now,
                         want_on);
      }
    }
  } else {
    LOG(LL_DEBUG, ("Control is disabled"));
  }
  for (Output *o : outputs_) {
    if (!o->IsValid()) continue;
    bool want_on = (want_outputs_on.find(o) != want_outputs_on.end());
    bool is_on = o->GetState();
    std::string n = o->name();
    LOG(LL_DEBUG,
        ("%s: is %s, want %s", n.c_str(), onoff(is_on), onoff(want_on)));
    if (want_on != is_on) {
      last_action_ts_ = now;
    }
    o->SetState(want_on);
  }
  last_eval_ = now;
}

bool Control::GetOutputStatus(const std::string &name_or_id, bool *on,
                              double *last_change) {
  const Output *o = GetOutputByNameOrID(name_or_id);
  if (o == nullptr || !o->IsValid()) return false;
  bool is_on = o->GetStateWithTimestamp(last_change);
  *on = is_on;
  return true;
}

void Control::ReportOutputs() {
  for (Output *o : outputs_) {
    o->Report();
  }
}

Limit *Control::GetLimitByID(const std::string &id) const {
  for (Limit *l : limits_) {
    if (std::to_string(l->id()) == id) {
      return l;
    }
  }
  return nullptr;
}

Output *Control::GetOutputByNameOrID(const std::string &name_or_id) const {
  for (Output *o : outputs_) {
    if (!o->IsValid()) continue;
    if (o->name() == name_or_id || std::to_string(o->id()) == name_or_id) {
      return o;
    }
  }
  return nullptr;
}

static void hub_heater_get_status_handler(struct mg_rpc_request_info *ri,
                                          void *cb_arg,
                                          struct mg_rpc_frame_info *fi,
                                          struct mg_str args) {
  int duration = -1;
  double now = cs_time();
  if (s_deadline >= now) {
    duration = (int) (s_deadline - now);
  }
  mg_rpc_send_responsef(ri, "{heater_on: %B, duration: %d}", s_heater_on,
                        duration);
  (void) args;
  (void) cb_arg;
  (void) fi;
}

static void hub_heater_set_handler(struct mg_rpc_request_info *ri, void *cb_arg,
                                   struct mg_rpc_frame_info *fi,
                                   struct mg_str args) {
  bool ctl_on = 0, heater_on = 0;
  int duration = -1;

  bool have_ctl = (json_scanf(args.p, args.len, "{ctl_on: %B}", &ctl_on) == 1);
  bool have_on =
      (json_scanf(args.p, args.len, "{heater_on: %B}", &heater_on) == 1);
  json_scanf(args.p, args.len, "{duration: %d}", &duration);

  if (!have_ctl && !have_on) {
    mg_rpc_send_errorf(ri, -1, "heater_on or ctl_on is required");
    goto out;
  }

  if (have_ctl) {
    s_ctl->SetEnabled(ctl_on, "request");
  }

  if (have_on) {
    if (duration <= 0) {
      duration = 12 * 3600;
    }

    if (heater_on != s_heater_on) {
      LOG(LL_INFO,
          ("Heater %s by request, duration %d", onoff(heater_on), duration));
      s_heater_on = heater_on;
    }

    double now = cs_time();
    s_deadline = now + duration;
  }

  mg_rpc_send_responsef(ri, "{ctl_on: %B, heater_on: %B, deadline: %.3lf}",
                        s_ctl->IsEnabled(), s_heater_on, s_deadline);

  s_ctl->Eval(true /* force */);

out:
  (void) cb_arg;
  (void) fi;
}

// static
void Control::GetLimitsRPCHandler(struct mg_rpc_request_info *ri, void *cb_arg,
                                  struct mg_rpc_frame_info *fi UNUSED_ARG,
                                  struct mg_str args) {
  Control *ctl = static_cast<Control *>(cb_arg);
  int sid = -1, subid = -1;

  json_scanf(args.p, args.len, ri->args_fmt, &sid, &subid);

  std::string res = "[";
  bool first = true;
  for (const Limit *l : ctl->limits_) {
    if (l->sid() < 0 || l->subid() < 0) continue;
    if (sid >= 0 && l->sid() != sid) continue;
    if (subid >= 0 && l->subid() != subid) continue;
    if (!first) res.append(", ");
    mgos::JSONAppendStringf(&res,
                            "{id: %d, sid: %d, subid: %d, enable: %B, "
                            "min: %.2lf, max: %.2lf, invert: %B, "
                            "deps: %Q, out: %Q}",
                            l->id(), l->sid(), l->subid(), l->enable(),
                            l->min(), l->max(), l->invert(),
                            l->DepsStr().c_str(), l->OutStr().c_str());
    first = false;
  }
  res.append("]");
  mg_rpc_send_responsef(ri, "%s", res.c_str());
}

// static
void Control::SetLimitRPCHandler(struct mg_rpc_request_info *ri, void *cb_arg,
                                 struct mg_rpc_frame_info *fi,
                                 struct mg_str args) {
  Control *ctl = static_cast<Control *>(cb_arg);
  char *msg = nullptr;
  int8_t enable = -1, invert = -1;
  int sid = -1, subid = 0;
  double min = NAN, max = NAN;
  char *deps_s = nullptr, *out_s = nullptr;

  json_scanf(args.p, args.len, ri->args_fmt, &sid, &subid, &enable, &min, &max,
             &invert, &deps_s, &out_s);

  mgos::ScopedCPtr deps_owner(deps_s), out_owner(out_s);

  if (sid < 0) {
    mg_rpc_send_errorf(ri, -1, "sid is required");
    return;
  }

  // Try to find an existing entry for this sensor first.
  Limit *l = nullptr;
  for (Limit *li : ctl->limits_) {
    if (li->sid() == sid && li->subid() == subid) {
      l = li;
      break;
    }
  }
  // If not found, find an unused/invalid entry.
  if (l == nullptr) {
    for (Limit *li : ctl->limits_) {
      if (!li->IsValid()) {
        l = li;
        break;
      }
    }
  }
  // If no unused entries, find a disabled one.
  if (l == nullptr) {
    for (Limit *li : ctl->limits_) {
      if (!li->enable()) {
        l = li;
        break;
      }
    }
  }
  if (l == nullptr) {
    mg_rpc_send_errorf(ri, -2, "no available entries");
    return;
  }

  if (deps_s != nullptr) {
    for (const std::string &id : Limit::ParseCommaStr(deps_s)) {
      Limit *l = ctl->GetLimitByID(id);
      if (l == nullptr) {
        mg_rpc_send_errorf(ri, -1, "invalid limit %s", id.c_str());
        return;
      }
    }
    l->set_deps(deps_s);
  }

  if (out_s != nullptr) {
    for (const std::string &name_or_id : Limit::ParseCommaStr(out_s)) {
      Output *o = ctl->GetOutputByNameOrID(name_or_id);
      if (o == nullptr) {
        mg_rpc_send_errorf(ri, -1, "invalid output %s", name_or_id.c_str());
        return;
      }
    }
    l->set_out(out_s);
  }
  l->set_sid(sid);
  l->set_subid(subid);
  if (enable != -1) l->set_enable(enable);
  if (!isnan(min)) l->set_min(min);
  if (!isnan(max)) l->set_max(max);
  if (invert != -1) l->set_invert(invert);
  LOG(LL_INFO, ("Modified limit: %s", l->ToString().c_str()));

  if (!mgos_sys_config_save(&mgos_sys_config, false /* try_once */, &msg)) {
    mg_rpc_send_errorf(ri, -1, "error saving config: %s", (msg ? msg : ""));
    free(msg);
    return;
  }

  ctl->Eval(true /* force */);

  mg_rpc_send_responsef(ri, nullptr);

  (void) fi;
}

// static
void Control::GetOutputsRPCHandler(struct mg_rpc_request_info *ri, void *cb_arg,
                                   struct mg_rpc_frame_info *fi UNUSED_ARG,
                                   struct mg_str args) {
  Control *ctl = static_cast<Control *>(cb_arg);
  int id = -1;
  char *name_s = nullptr;

  json_scanf(args.p, args.len, ri->args_fmt, &id, &name_s);

  std::unique_ptr<char> name_owner(name_s);
  std::string name(name_s ? name_s : "");

  std::string res = "[";
  bool first = true;
  for (const Output *o : ctl->outputs_) {
    if (!o->IsValid()) continue;
    if (id >= 0 && o->id() != id) continue;
    if (name.length() > 0 && o->name() != name) continue;

    const std::string &n = o->name();
    const std::string &pin_name = o->pin_name();
    if (!first) res.append(", ");
    mgos::JSONAppendStringf(
        &res, "{id: %d, name: %Q, pin: %d, pin_name: %Q, act: %d, on: %B}",
        o->id(), n.c_str(), o->pin(), pin_name.c_str(), o->act(),
        o->GetState());
    first = false;
  }

  res.append("]");
  mg_rpc_send_responsef(ri, "%s", res.c_str());
}

static void heater_crontab_cb(struct mg_str action, struct mg_str payload,
                              void *userdata) {
#if 0  // TODO
  bool heater_on = (bool) (intptr_t) userdata;

  LOG(LL_INFO, ("Heater %s by cron", onoff(heater_on)));

  double now = cs_time();
  s_deadline = now + 12 * 3600;  // XXX: For now
  s_last_action_ts = now;
  s_heater_on = heater_on;
#endif
  (void) userdata;
  (void) payload;
  (void) action;
}

static void ctl_crontab_cb(struct mg_str action, struct mg_str payload,
                           void *userdata) {
  bool ctl_on = (bool) (intptr_t) userdata;

  if (s_ctl->IsEnabled() == ctl_on) return;

  s_ctl->SetEnabled(ctl_on, "cron");

  (void) payload;
  (void) action;
}

void HubControlReportOutputs() {
  if (s_ctl != nullptr) s_ctl->ReportOutputs();
}

bool HubControlInit() {
  bool res = false;
  struct mg_rpc *c = mgos_rpc_get_global();
  const struct mgos_config_hub_control *ccfg =
      &mgos_sys_config_get_hub()->control;

  s_ctl = new Control(const_cast<struct mgos_config_hub_control *>(ccfg));

  mg_rpc_add_handler(c, "Hub.Control.GetLimits", "{sid: %d, subid: %d}",
                     Control::GetLimitsRPCHandler, s_ctl);
  mg_rpc_add_handler(
      c, "Hub.Control.SetLimit",
      "{sid: %d, subid: %d, enable: %B, min: %lf, max: %lf, invert: %B, "
      "deps: %Q, out: %Q}",
      Control::SetLimitRPCHandler, s_ctl);
  mg_rpc_add_handler(c, "Hub.Control.GetOutputs", "{id: %d, name: %Q}",
                     Control::GetOutputsRPCHandler, s_ctl);
  // Backward-compat.
  mg_rpc_add_handler(c, "Hub.Heater.GetStatus", "",
                     hub_heater_get_status_handler, nullptr);
  mg_rpc_add_handler(c, "Hub.Heater.Set",
                     "{ctl_on: %B, heater_on: %B, duration: %d}",
                     hub_heater_set_handler, nullptr);
  mg_rpc_add_handler(c, "Hub.Heater.GetLimits", "{sid: %d, subid: %d}",
                     Control::GetLimitsRPCHandler, s_ctl);
  mg_rpc_add_handler(
      c, "Hub.Heater.SetLimits",
      "{sid: %d, subid: %d, enable: %B, min: %lf, max: %lf, invert: %B, "
      "deps: %Q, out: %Q}",
      Control::SetLimitRPCHandler, s_ctl);

  mgos_crontab_register_handler(mg_mk_str("heater_on"), heater_crontab_cb,
                                (void *) 1);
  mgos_crontab_register_handler(mg_mk_str("heater_off"), heater_crontab_cb,
                                (void *) 0);
  mgos_crontab_register_handler(mg_mk_str("ctl_on"), ctl_crontab_cb,
                                (void *) 1);
  mgos_crontab_register_handler(mg_mk_str("ctl_off"), ctl_crontab_cb,
                                (void *) 0);

  res = true;

  return res;
}

bool HubControlGetHeaterStatus(bool *heater_on, double *last_action_ts) {
  return s_ctl->GetOutputStatus("Heater", heater_on, last_action_ts);
}
