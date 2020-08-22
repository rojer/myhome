#pragma once

#include <string>

struct mgos_config_hub_control_output;

class Output {
 public:
  explicit Output(struct mgos_config_hub_control_output *o);
  int id() const;
  std::string name() const;
  int pin() const;
  std::string pin_name() const;
  int act() const;
  bool IsValid() const;
  bool GetState() const;
  bool GetStateWithTimestamp(double *last_change) const;
  void SetState(bool new_state);
  void Report();

 private:
  struct mgos_config_hub_control_output *o_;

  bool on_;
  double last_change_;
};
