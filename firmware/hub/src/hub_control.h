#pragma once

#include <string>
#include <vector>

#include "mgos_timers.hpp"

#include "hub_control_limit.h"
#include "hub_control_output.h"

class Control {
 public:
  explicit Control(struct mgos_config_hub_control *cfg);

  bool IsEnabled() const;
  void SetEnabled(bool enable, const std::string &source);
  void Eval(bool force = false);
  void ReportOutputs();
  bool GetOutputStatus(const std::string &name_or_id, bool *on,
                       double *last_change);

  static void GetLimitsRPCHandler(struct mg_rpc_request_info *ri, void *cb_arg,
                                  struct mg_rpc_frame_info *fi,
                                  struct mg_str args);
  static void SetLimitRPCHandler(struct mg_rpc_request_info *ri, void *cb_arg,
                                 struct mg_rpc_frame_info *fi,
                                 struct mg_str args);
  static void GetOutputsRPCHandler(struct mg_rpc_request_info *ri, void *cb_arg,
                                   struct mg_rpc_frame_info *fi,
                                   struct mg_str args);

 private:
  Output *GetOutputByNameOrID(const std::string &name_or_id) const;

  struct mgos_config_hub_control *cfg_;
  std::vector<Limit *> limits_;
  std::vector<Output *> outputs_;
  mgos::Timer eval_timer_;
  double last_action_ts_ = 0.0;
  double last_eval_ = 0.0;
};

bool HubControlGetHeaterStatus(bool *heater_on, double *last_action_ts);
void HubControlReportOutputs();
bool HubControlInit();
