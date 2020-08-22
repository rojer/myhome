#pragma once

#include <set>
#include <string>

struct mgos_config_hub_control_limit;

class Limit {
 public:
  explicit Limit(struct mgos_config_hub_control_limit *l);
  static std::set<std::string> ParseOutputsStr(const std::string &out);

  int sid() const;
  void set_sid(int sid) const;
  int subid() const;
  void set_subid(int subid) const;
  bool enable() const;
  void set_enable(bool enable);
  double min() const;
  void set_min(double min);
  double max() const;
  void set_max(double max);
  std::string out() const;
  void set_out(const std::string &out);

  std::set<std::string> outputs() const;
  bool IsValid();
  bool Eval();

 private:
  struct mgos_config_hub_control_limit *l_;

  bool on_;
  double last_change_;
};
