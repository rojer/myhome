#pragma once

#include <cstdint>
#include <string>

#define UPTIME_SUBID 0
#define HEAP_FREE_SUBID 1

#define TEMP_SUBID 0
#define RH_SUBID 1

struct SensorData {
  int sid = -1;
  int subid = -1;
  double ts = 0.0;
  double value = 0.0;
  std::string name;

  SensorData() = default;
  SensorData(int sid, int subid, double ts, double value);
  uint64_t GetKey() const;
  static uint64_t MakeKey(int sid, int subid);
  std::string ToString() const;
};

void report_to_server(int sid, int subid, double ts, double value);
void hub_add_data(const struct SensorData *sd);
bool hub_get_data(int sid, int subid, struct SensorData *sd);

bool hub_data_init(void);
