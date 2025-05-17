#pragma once

#include <memory>
#include <vector>

#include "shos_bt_addr.hpp"
#include "shos_bt_gap_adv.hpp"
#include "shos_bt_uuid.hpp"
#include "shos_str.hpp"

// #include "shelly_common.hpp"

namespace bthome {

using shos::Errorf;
using shos::Status;
using shos::StatusOr;
using shos::Str;

static inline Status InternalError(const char *msg) {
  return Status(STATUS_UNKNOWN, msg);
}

static constexpr shos::bt::UUID kBTHomeServiceUUID = 0xFCD2;
static constexpr uint8_t kObjectIDPacketID = 0x00;
static constexpr uint8_t kObjectIDBattery = 0x01;
static constexpr uint8_t kObjectIDTemperature1 = 0x02;
static constexpr uint8_t kObjectIDTemperature2 = 0x45;
static constexpr uint8_t kObjectIDHumidity1 = 0x03;
static constexpr uint8_t kObjectIDHumidity2 = 0x2E;
static constexpr uint8_t kObjectIDIlluminance = 0x05;
static constexpr uint8_t kObjectIDWindow = 0x2D;
static constexpr uint8_t kObjectIDButton = 0x3A;
static constexpr uint8_t kObjectIDDimmer = 0x3C;
static constexpr uint8_t kObjectIDRotation = 0x3F;
static constexpr uint8_t kObjectIDText = 0x53;
static constexpr uint8_t kObjectIDRaw = 0x54;

enum class DataType {
  kAny,
  kSensor,
  kBinarySensor,
  kRawSensor,
  kEvent,
  kOther,
};

enum class DataFormat {
  kNone,
  kUnsignedInt,
  kSignedInt,
  kFloat,
  kBool,
  kStr,
};

// https://bthome.io/format/ -> Events
enum class ButtonEvents {
  kNone = 0x00,
  kPress = 0x01,
  kDoublePress = 0x02,
  kTripplePress = 0x03,
  kLongPress = 0x04,
  kLongDoublePress = 0x05,
  kLongTripplePress = 0x06,
  kHoldPress = 0x80,
  kButtonPress = 0xFE,
};

// https://bthome.io/format/ -> Events
enum class DimmerEvents {
  kNone = 0x00,
  kRotateLeft = 0x01,
  kRotateRight = 0x02,
};

enum class Events {
  kButton = kObjectIDButton,
  kDimmer = kObjectIDDimmer,
};

struct BTHomeValue {
  uint8_t obj_id;
  uint8_t index;
  DataType type;
  DataFormat format;
  union {
    uint32_t unsigned_val;
    int32_t int_val;
    float float_val;
    bool bool_val;
    struct {
      uint8_t len;
      uint8_t val[3];  // Note: Longer values are truncated to 3 bytes.
    } str_val;
  };

  Str GetStrVal() const { return Str(str_val.val, str_val.len); }

  static StatusOr<BTHomeValue> ParseAndConsume(Str &data);

  std::string ToString() const;

  bool operator==(const BTHomeValue &other) const;
  bool operator!=(const BTHomeValue &other) const { return !(*this == other); }
};

struct BTHomeObject {
  uint8_t id;
  DataType type;
  const char *name;
  DataFormat format;
  size_t data_length;
  int exponent;
  const char *unit;

  static StatusOr<BTHomeObject> Get(uint8_t id);
  static StatusOr<BTHomeObject> Get(const std::string &name, int order = 0);
  static std::string GetName(uint8_t id);
  static uint8_t GetId(const std::string &name, int order = 0);
  static bool IsSensor(uint8_t id);
  static bool IsBinarySensor(uint8_t id);
  static bool IsEvent(uint8_t id);
  static DataType GetDataType(uint8_t id);
  static std::string GetUnit(uint8_t id);
  static int GetPrecision(uint8_t id);
  static std::string DataTypeString(uint8_t id, DataType type);

  std::string GetInfoJson() const;

 private:
  static const BTHomeObject s_bthome_objects[];
};

struct BTHomeData {
  enum class ParseErrors {
    kKeyMissingOrBad = -201,
    kDecryptFailed = -202,
    kParseFailed = -203,
    kUnencrypted = -204,
  };

  union DeviceInfo {
    struct __attribute__((packed)) {
      uint8_t encrypted : 1;
      uint8_t unused_1 : 1;
      uint8_t trigger_based : 1;
      uint8_t unused_2 : 2;
      uint8_t version : 3;
    };
    uint8_t byte = 0;
  };

  BTHomeData() = default;

  shos::bt::Addr address;
  DeviceInfo info;
  std::string bthome_data;
  std::vector<BTHomeValue> values;

  StatusOr<BTHomeValue> GetValue(uint8_t obj_id, uint8_t index) const;

  std::string ToString() const;

  Status Parse(const shos::bt::Addr &addr, const shos::bt::gap::AdvData &ad,
               Str key = Str());
  Status Parse(const shos::bt::Addr &addr, Str data, Str key = Str());

 private:
  static constexpr const int supported_version = 2;

  BTHomeData(const shos::bt::Addr &addr) : address(addr) {}

  Status Decrypt(Str encrypted, Str key);
  Status ParseData();
};

}  // namespace bthome
