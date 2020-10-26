#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UPTIME_SUBID 0
#define HEAP_FREE_SUBID 1

#define TEMP_SUBID 0
#define RH_SUBID 1

struct sensor_data {
  int sid;
  int subid;
  char *name;
  double ts;
  double value;
};

void report_to_server(int sid, int subid, double ts, double value);
void report_to_server_sd(const struct sensor_data *sd);
void hub_add_data(const struct sensor_data *sd);
bool hub_get_data(int sid, int subid, struct sensor_data *sd);

bool hub_data_init(void);

#ifdef __cplusplus
}
#endif
