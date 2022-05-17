#pragma once

#include <set>
#include <string>

struct mgos_config_hub_control_limit;

class Limit {
 public:
  Limit(int id, struct mgos_config_hub_control_limit *l);

  static std::set<std::string> ParseCommaStr(const std::string &out);

  int id() const;
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
  bool invert() const;
  void set_invert(bool invert);
  std::string DepsStr() const;
  void set_deps(const std::string &out);
  std::string OutStr() const;
  void set_out(const std::string &out);

  std::string ToString() const;

  std::set<std::string> deps() const;
  std::set<std::string> outputs() const;
  bool IsValid();
  bool Eval(bool quiet = false);

 private:
  const int id_;
  struct mgos_config_hub_control_limit *l_;

  bool on_;
  double last_change_;
};
