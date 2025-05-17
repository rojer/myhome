#include "BTHomeData.hpp"

#include <cmath>
#include <cstring>

#include "shos_json_utils.hpp"
#include "shos_utils.hpp"

#include "mbedtls/ccm.h"

namespace bthome {

// clang-format off
const BTHomeObject BTHomeObject::s_bthome_objects[] = {
    // Packet id
    {kObjectIDPacketID, DataType::kOther, "packet_id", DataFormat::kUnsignedInt, 1, 0, ""},
    // Sensors
    {kObjectIDBattery, DataType::kSensor, "battery", DataFormat::kUnsignedInt, 1, 0, "%"},
    {0x02, DataType::kSensor, "temperature", DataFormat::kSignedInt, 2, -2, "\xC2\xB0 C"},
    {0x03, DataType::kSensor, "humidity", DataFormat::kUnsignedInt, 2, -2, "%"},
    {0x04, DataType::kSensor, "pressure", DataFormat::kUnsignedInt, 3, -2, "hPa"},
    {0x05, DataType::kSensor, "illuminance", DataFormat::kUnsignedInt, 3, -2, "lux"},
    {0x06, DataType::kSensor, "mass_kg", DataFormat::kUnsignedInt, 2, -2, "kg"},
    {0x07, DataType::kSensor, "mass_lb", DataFormat::kUnsignedInt, 2, -2, "lb"},
    {0x08, DataType::kSensor, "dewpoint", DataFormat::kSignedInt, 2, -2, "\xC2\xB0 C"},
    {0x09, DataType::kSensor, "count", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x0A, DataType::kSensor, "energy", DataFormat::kUnsignedInt, 3, -3, "kWh"},
    {0x0B, DataType::kSensor, "power", DataFormat::kUnsignedInt, 3, -2, "W"},
    {0x0C, DataType::kSensor, "voltage", DataFormat::kUnsignedInt, 2, -3, "V"},
    {0x0D, DataType::kSensor, "pm2.5", DataFormat::kUnsignedInt, 2, 0, "ug/m3"},
    {0x0E, DataType::kSensor, "pm10", DataFormat::kUnsignedInt, 2, 0, "ug/m3"},
    {0x12, DataType::kSensor, "co2", DataFormat::kUnsignedInt, 2, 0, "ppm"},
    {0x13, DataType::kSensor, "tvoc", DataFormat::kUnsignedInt, 2, 0, "ug/m3"},
    {0x14, DataType::kSensor, "moisture", DataFormat::kUnsignedInt, 2, -2, "%"},
    {0x2E, DataType::kSensor, "humidity", DataFormat::kUnsignedInt, 1, 0, "%"},
    {0x2F, DataType::kSensor, "moisture", DataFormat::kUnsignedInt, 1, 0, "%"},
    {0x3D, DataType::kSensor, "count", DataFormat::kUnsignedInt, 2, 0, ""},
    {0x3E, DataType::kSensor, "count", DataFormat::kUnsignedInt, 4, 0, ""},
    {0x3F, DataType::kSensor, "rotation", DataFormat::kSignedInt, 2, -1, "\xC2\xB0"},
    {0x40, DataType::kSensor, "distance_mm", DataFormat::kUnsignedInt, 2, 0, "mm"},
    {0x41, DataType::kSensor, "distance_m", DataFormat::kUnsignedInt, 2, -1, "m"},
    {0x42, DataType::kSensor, "duration", DataFormat::kUnsignedInt, 3, -3, "s"},
    {0x43, DataType::kSensor, "current", DataFormat::kUnsignedInt, 2, -3, "A"},
    {0x44, DataType::kSensor, "speed", DataFormat::kUnsignedInt, 2, -2, "m/s"},
    {0x45, DataType::kSensor, "temperature", DataFormat::kSignedInt, 2, -1, "\xC2\xB0 C"},
    {0x46, DataType::kSensor, "uv_index", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x47, DataType::kSensor, "volume", DataFormat::kUnsignedInt, 2, -1, "L"},
    {0x48, DataType::kSensor, "volume", DataFormat::kUnsignedInt, 2, 0, "mL"},
    {0x49, DataType::kSensor, "volume_flow_rate", DataFormat::kUnsignedInt, 2, -3, "m3/hr"},
    {0x4A, DataType::kSensor, "voltage", DataFormat::kUnsignedInt, 2, -1, "V"},
    {0x4B, DataType::kSensor, "gas", DataFormat::kUnsignedInt, 3, -3, "m3"},
    {0x4C, DataType::kSensor, "gas", DataFormat::kUnsignedInt, 4, -3, "m3"},
    {0x4D, DataType::kSensor, "energy", DataFormat::kUnsignedInt, 4, -3, "kWh"},
    {0x4E, DataType::kSensor, "volume", DataFormat::kUnsignedInt, 4, -3, "L"},
    {0x4F, DataType::kSensor, "water", DataFormat::kUnsignedInt, 4, -3, "L"},
    {0x50, DataType::kSensor, "timestamp", DataFormat::kUnsignedInt, 4, 0, ""},
    {0x51, DataType::kSensor, "acceleration", DataFormat::kUnsignedInt, 2, -3, "m/s2"},
    {0x52, DataType::kSensor, "gyroscope", DataFormat::kUnsignedInt, 2, -3, "\xC2\xB0/s"},
    // Binary sensors
    {0x0F, DataType::kBinarySensor, "generic_boolean", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x10, DataType::kBinarySensor, "power_status", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x11, DataType::kBinarySensor, "opening", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x15, DataType::kBinarySensor, "battery_status", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x16, DataType::kBinarySensor, "battery_charging", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x17, DataType::kBinarySensor, "carbon_monoxide", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x18, DataType::kBinarySensor, "cold", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x19, DataType::kBinarySensor, "connectivity", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x1A, DataType::kBinarySensor, "door", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x1B, DataType::kBinarySensor, "garage_door", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x1C, DataType::kBinarySensor, "gas", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x1D, DataType::kBinarySensor, "heat", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x1E, DataType::kBinarySensor, "light", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x1F, DataType::kBinarySensor, "lock", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x20, DataType::kBinarySensor, "moisture", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x21, DataType::kBinarySensor, "motion", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x22, DataType::kBinarySensor, "moving", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x23, DataType::kBinarySensor, "occupancy", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x24, DataType::kBinarySensor, "plug", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x25, DataType::kBinarySensor, "presence", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x26, DataType::kBinarySensor, "problem", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x27, DataType::kBinarySensor, "running", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x28, DataType::kBinarySensor, "safety", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x29, DataType::kBinarySensor, "smoke", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x2A, DataType::kBinarySensor, "sound", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x2B, DataType::kBinarySensor, "tamper", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x2C, DataType::kBinarySensor, "vibration", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x2D, DataType::kBinarySensor, "window", DataFormat::kUnsignedInt, 1, 0, ""},
    // Events
    {0x3A, DataType::kEvent, "button", DataFormat::kUnsignedInt, 1, 0, ""},
    {0x3C, DataType::kEvent, "dimmer", DataFormat::kUnsignedInt, 2, 0, ""},
    // String-valued objects
    {kObjectIDText, DataType::kRawSensor, "text", DataFormat::kStr, 0, 0, ""},
    {kObjectIDRaw, DataType::kRawSensor, "raw", DataFormat::kStr, 0, 0, ""},
    // Device information
    {0xF0, DataType::kOther, "device_type_id", DataFormat::kUnsignedInt, 2, 0, ""},
    {0xF1, DataType::kOther, "firmware_version", DataFormat::kUnsignedInt, 4, 0, ""},
    {0xF2, DataType::kOther, "firmware_version", DataFormat::kUnsignedInt, 3, 0, ""},
};
// clang-format on

bool BTHomeValue::operator==(const BTHomeValue &other) const {
  if (static_cast<int>(format) != static_cast<int>(other.format)) {
    return false;
  }
  switch (format) {
    case DataFormat::kNone: return (other.format == DataFormat::kNone);
    case DataFormat::kUnsignedInt: return (unsigned_val == other.unsigned_val);
    case DataFormat::kSignedInt: return (int_val == other.int_val);
    case DataFormat::kFloat: return (float_val == other.float_val);
    case DataFormat::kBool: return (bool_val == other.bool_val);
    case DataFormat::kStr: return (GetStrVal() == other.GetStrVal());
  }
  return false;
}

StatusOr<BTHomeObject> BTHomeObject::Get(uint8_t id) {
  for (auto &obj : s_bthome_objects) {
    if (obj.id == id) return obj;
  }
  return InternalError(
      shos::SPrintf("Unknown BTHome object (0x%02X)", id).c_str());
}

StatusOr<BTHomeObject> BTHomeObject::Get(const std::string &name, int order) {
  int n = 0;
  for (auto &obj : s_bthome_objects) {
    if (name == obj.name) {
      if (n == order) return obj;
      n++;
    }
  }
  return InternalError(
      shos::SPrintf("Unknown BTHome object (%s)", name.c_str()).c_str());
}

std::string BTHomeObject::GetName(uint8_t id) {
  auto obj = Get(id);
  if (!obj.ok()) {
    return "unknown";
  }
  return obj.ValueOrDie().name;
}

uint8_t BTHomeObject::GetId(const std::string &name, int order) {
  auto obj = Get(name, order);
  if (!obj.ok()) {
    return UINT8_MAX;
  }
  return obj.ValueOrDie().id;
}

bool BTHomeObject::IsSensor(uint8_t id) {
  auto obj = Get(id);
  if (!obj.ok()) {
    return false;
  }
  return (obj.ValueOrDie().type == DataType::kSensor);
}

bool BTHomeObject::IsBinarySensor(uint8_t id) {
  auto obj = Get(id);
  if (!obj.ok()) {
    return false;
  }
  return (obj.ValueOrDie().type == DataType::kBinarySensor);
}

bool BTHomeObject::IsEvent(uint8_t id) {
  auto obj = Get(id);
  if (!obj.ok()) {
    return false;
  }
  return (obj.ValueOrDie().type == DataType::kEvent);
}

DataType BTHomeObject::GetDataType(uint8_t id) {
  auto obj = Get(id);
  if (!obj.ok()) {
    return DataType::kAny;
  }
  return obj.ValueOrDie().type;
}

std::string BTHomeObject::GetUnit(uint8_t id) {
  auto obj = Get(id);
  if (!obj.ok()) {
    return "";
  }
  return obj.ValueOrDie().unit;
}

int BTHomeObject::GetPrecision(uint8_t id) {
  auto obj = Get(id);
  if (!obj.ok()) {
    return 0;
  }
  int exponent = obj.ValueOrDie().exponent;
  return exponent >= 0 ? 0 : std::abs(exponent);
}

std::string BTHomeObject::DataTypeString(uint8_t id, DataType type) {
  switch (type) {
    case DataType::kSensor: return "sensor";
    case DataType::kBinarySensor: return "binary_sensor";
    case DataType::kEvent: {
      switch (static_cast<Events>(id)) {
        case Events::kButton: return "button";
        case Events::kDimmer: return "dimmer";
        default: return "other";
      }
    } break;
    default: return "other";
  }
}

std::string BTHomeObject::GetInfoJson() const {
  return shos::json::SPrintf("{obj_id:%u,obj_name:%Q,type:%Q,unit:%Q}", id,
                             name, DataTypeString(id, type).c_str(), unit);
}

StatusOr<BTHomeValue> BTHomeValue::ParseAndConsume(Str &data) {
  if (data.size() < 2) return Status::INVALID_ARGUMENT();
  const uint8_t obj_id = data[0];
  size_t obj_value_len;
  const auto &objv = BTHomeObject::Get(obj_id);
  if (!objv.ok()) {
    return Errorf(static_cast<int>(BTHomeData::ParseErrors::kParseFailed),
                  "Unknown BTHome object id (0x%02X)", obj_id);
  }
  const BTHomeObject &obj = objv.ValueOrDie();
  if (obj.format == DataFormat::kStr) {
    obj_value_len = 1 /* raw data len */ + data[1];
  } else {
    obj_value_len = obj.data_length;
  }
  const size_t obj_len = 1 /* obj_id */ + obj_value_len;
  if (data.size() < obj_len) {
    return Errorf(static_cast<int>(BTHomeData::ParseErrors::kParseFailed),
                  "Invalid BTHome object data length (0x%02X %zu - %zu)",
                  obj_id, obj_len, data.size());
  }
  if (obj.format == DataFormat::kStr) {
    const uint8_t to_copy =
        std::min(uint8_t(data[1]), uint8_t(sizeof(str_val.val)));
    BTHomeValue res{
        .obj_id = obj_id,
        .index = 0,
        .type = DataType::kOther,
        .format = DataFormat::kStr,
        .str_val = {.len = to_copy, .val = {}},
    };
    std::memcpy(res.str_val.val, data.p + 2, to_copy);
    data.ChopLeft(obj_len);
    return res;
  }
  const bool is_signed = (obj.format == DataFormat::kSignedInt);
  union {
    uint32_t u32;
    int32_t i32;
    uint16_t u16;
    int16_t i16;
    uint8_t u8;
    int8_t i8;
  } uv = {((*(data.p + obj.data_length) & 0x80) ? UINT32_MAX : 0)};
  std::memcpy(&uv, data.p + 1, obj.data_length);
  data.ChopLeft(obj_len);
  long long raw = 0;
  switch (obj.data_length) {
    // There is no (u)int24 type but there are 3byte bthome objects.
    case 3:
    case 4: raw = (is_signed ? uv.i32 : uv.u32); break;
    case 2: raw = (is_signed ? uv.i16 : uv.u16); break;
    case 1: raw = (is_signed ? uv.i8 : uv.u8); break;
  }
  if (obj.type == DataType::kSensor) {
    return BTHomeValue{
        .obj_id = obj_id,
        .index = 0,
        .type = obj.type,
        .format = DataFormat::kFloat,
        .float_val = static_cast<float>(raw * pow(10, obj.exponent)),
    };
  }
  if (obj.type == DataType::kBinarySensor) {
    return BTHomeValue{
        .obj_id = obj_id,
        .index = 0,
        .type = obj.type,
        .format = DataFormat::kBool,
        .bool_val = (raw != 0),
    };
  }
  switch (obj.format) {
    case DataFormat::kUnsignedInt:
      return BTHomeValue{
          .obj_id = obj_id,
          .index = 0,
          .type = obj.type,
          .format = DataFormat::kUnsignedInt,
          .unsigned_val = uint32_t(raw),
      };
    case DataFormat::kSignedInt:
      return BTHomeValue{
          .obj_id = obj_id,
          .index = 0,
          .type = obj.type,
          .format = DataFormat::kSignedInt,
          .int_val = int32_t(raw),
      };
    default:
      return InternalError(
          shos::SPrintf("Invalid BTHome object data format (%d)",
                        static_cast<int>(obj.format))
              .c_str());
  }
}

Status BTHomeData::Parse(const shos::bt::Addr &addr,
                         const shos::bt::gap::AdvData &ad, Str key) {
  const Str bthome_service_data = ad.GetServiceData(kBTHomeServiceUUID);
  if (bthome_service_data.empty()) {
    // LOG(LL_INFO, ("No BTHome data"));
    return Status::NOT_FOUND();
  }
  return Parse(addr, bthome_service_data, key);
}

Status BTHomeData::Parse(const shos::bt::Addr &addr, Str data, Str key) {
  address = addr;
  if (data.empty()) return Status::INVALID_ARGUMENT();
  info.byte = data[0];
  if (info.version != supported_version) {
    return Errorf(static_cast<int>(ParseErrors::kParseFailed),
                  "Unsupported BTHome version (%d)", info.version);
  }
  if (info.encrypted) {
    // auto st = Decrypt(data, key);
    // if (!st.ok()) return Annotatef(st, "%s failed", "decrypt");
  } else if (!key.empty()) {
    return Errorf(static_cast<int>(ParseErrors::kUnencrypted),
                  "Unencrypted data for encrypted BTHome device");
  } else {
    bthome_data = data.substr(1).ToString();
  }

  return ParseData();
}

#if 0
Status BTHomeData::Decrypt(Str encrypted, Str key) {
  unsigned char key_bytes[AES_KEY_SIZE];
  if (!BTHomeDevice::AesKeyFromString(key, key_bytes)) {
    return Errorf(static_cast<int>(ParseErrors::kKeyMissingOrBad),
                  "BTHome decrypt invalid key");
  }
  if (encrypted.size() < 15) {
    return Errorf(static_cast<int>(ParseErrors::kParseFailed),
                  "BTHome decrypt invalid encrypted data length");
  }

  mbedtls_ccm_context ctx;
  mbedtls_ccm_init(&ctx);
  mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key_bytes, 128);

  unsigned char nonce[13];
  memcpy(&nonce[0], address.addr, sizeof(address.addr));  // BT address
  const uint16_t uuid16 = kShellyBTHomeServiceUUID.To16();
  memcpy(&nonce[6], &uuid16, sizeof(uuid16));  // BTHome UUID
  nonce[8] = encrypted.p[0];                   // BTHome device info
  memcpy(&nonce[9], encrypted.p + (encrypted.len - 8), 4);  // Counter
  unsigned char tag[4];
  memcpy(tag, encrypted.p + (encrypted.len - 4), 4);
  size_t data_len = encrypted.len - 8 - 1;
  char *decrypted_str = (char *) malloc(data_len);
  if (decrypted_str == nullptr) {
    return Errorf(static_cast<int>(ParseErrors::kDecryptFailed),
                  "BTHome decrypt out of memory");
  }
  int result = mbedtls_ccm_auth_decrypt(
      &ctx, data_len, nonce, sizeof(nonce), NULL, 0,
      (unsigned char *) encrypted.p + 1, (unsigned char *) decrypted_str, tag,
      sizeof(tag));
  mbedtls_ccm_free(&ctx);

  if (result != 0) {
    free(decrypted_str);
    return Errorf(static_cast<int>(ParseErrors::kDecryptFailed),
                  "BTHome decrypt failed (%d)", result);
  }
  bthome_data = Str(decrypted_str, data_len);
  return Status::OK();
}
#endif

Status BTHomeData::ParseData() {
  Str data = bthome_data;
  while (data.size() >= 2) {
    auto rv = BTHomeValue::ParseAndConsume(data);
    if (!rv.ok()) {
      return Errorf(static_cast<int>(ParseErrors::kParseFailed), "%s",
                    rv.status().error_message().c_str());
    }
    BTHomeValue value = rv.MoveValueOrDie();
    for (const auto &v : values) {
      if (v.obj_id == value.obj_id) value.index++;
    }
    values.emplace_back(value);
  }
  values.shrink_to_fit();
  return Status::OK();
}

StatusOr<BTHomeValue> BTHomeData::GetValue(uint8_t obj_id,
                                           uint8_t index) const {
  for (const auto &v : values) {
    if (v.obj_id != obj_id) continue;
    if (index == 0) return v;
    index--;
  }
  return Status::NOT_FOUND();
}

std::string BTHomeData::ToString() const {
  auto out = shos::SPrintf(
      "BTHomeData(%s): v %d enc %d '%s' [", address.ToString(false).c_str(),
      info.version, info.encrypted, Str(bthome_data).ToHexString(true).c_str());
  bool first = true;
  for (const auto &val : values) {
    if (!first) out += " ";
    out += val.ToString();
    first = false;
  }
  out += "]";
  return out;
}

std::string BTHomeValue::ToString() const {
  std::string vs;
  switch (format) {
    case DataFormat::kNone: vs = "none"; break;
    case DataFormat::kUnsignedInt:
      vs = shos::SPrintf("%u", unsigned(unsigned_val));
      break;
    case DataFormat::kSignedInt: vs = shos::SPrintf("%d", int(int_val)); break;
    case DataFormat::kFloat: vs = shos::SPrintf("%f", float_val); break;
    case DataFormat::kBool:
      vs = shos::SPrintf("%s", (bool_val ? "true" : "false"));
      break;
    case DataFormat::kStr:
      if (obj_id == kObjectIDText) {
        vs = GetStrVal().ToString();
      } else {
        vs = GetStrVal().ToHexString();
      }
      break;
  }
  return shos::SPrintf("%s:%d=%s", BTHomeObject::GetName(obj_id).c_str(), index,
                       vs.c_str());
}

}  // namespace bthome
