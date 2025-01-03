/*
 * Copyright (c) 2024 Shelly Group
 * All rights reserved
 */

#pragma once

#include <initializer_list>

#include "mgos_bt.h"
#include "shos_str.hpp"

#include "host/ble_uuid.h"

namespace shos::bt {

struct shos_bt_uuid : public mgos_bt_uuid {
  constexpr shos_bt_uuid(uint16_t u16)
      : mgos_bt_uuid{
            .uuid = {.uuid16 = u16},
            .len = 2,
        } {
    // xxxxxxxx-0000-1000-8000-00805f9b34fb, as per spec.
    // uuid32_pad{0x5f9b34fb, 0x80000080, 0x00001000},
  }
  constexpr shos_bt_uuid()
      : mgos_bt_uuid{
            .uuid = {},
            .len = 0,
        } {}
  // Note: This is big-endian
  constexpr shos_bt_uuid(std::initializer_list<uint8_t> l128)
      : mgos_bt_uuid{
            .uuid = {.uuid128 = {*(l128.begin() + 15), *(l128.begin() + 14),
                                 *(l128.begin() + 13), *(l128.begin() + 12),
                                 *(l128.begin() + 11), *(l128.begin() + 10),
                                 *(l128.begin() + 9), *(l128.begin() + 8),
                                 *(l128.begin() + 7), *(l128.begin() + 6),
                                 *(l128.begin() + 5), *(l128.begin() + 4),
                                 *(l128.begin() + 3), *(l128.begin() + 2),
                                 *(l128.begin() + 1), *(l128.begin() + 0)}},
            .len = 16} {}
};

struct UUID : public shos_bt_uuid {
  constexpr UUID() : shos_bt_uuid() {}
  constexpr UUID(int u16_or_u32) : shos_bt_uuid((uint32_t) u16_or_u32) {}
  constexpr UUID(uint32_t u16_or_u32) : shos_bt_uuid(u16_or_u32) {}
  constexpr UUID(std::initializer_list<uint8_t> l128) : shos_bt_uuid(l128) {}
  UUID(const shos_bt_uuid &other) {
    std::memcpy(uuid.uuid128, other.uuid.uuid128, 16);
    len = other.len;
  }
  UUID(const shos_bt_uuid *other) : UUID(*other) {}
  UUID(const void *uuid, size_t len);  // Little-endian representation.
  UUID(const char uuid_str[]) { mgos_bt_uuid_from_str(Str(uuid_str), this); }
  UUID(Str uuid_str) { mgos_bt_uuid_from_str(uuid_str, this); }

  UUID(const ble_uuid_any_t &uuid);
  void ToNimBLE(ble_uuid_any_t &uuid) const;

  bool IsZero() const { return mgos_bt_uuid_is_zero(this); }
  bool IsValid() const;
  size_t Len() const { return len; }

  uint16_t To16() const;        // If not a 16-bit UUID, retruns 0.
  uint32_t To32() const;        // If not a 32-bit UUID, returns 0.
  std::string ToBytes() const;  // 0, 2, 4 or 16 bytes, LE.

#if 0
  static constexpr uint32_t kStringUpper = SHOS_BT_UUID_STRING_UPPER;
  static constexpr uint32_t kStringForce128 = SHOS_BT_UUID_STRING_FORCE_128;
#endif
  std::string ToString(uint32_t flags = 0) const;

  friend bool operator<(const UUID &a, const UUID &b) {
    return (mgos_bt_uuid_cmp(&a, &b) < 0);
  }
  friend bool operator==(const UUID &a, const UUID &b) {
    return (mgos_bt_uuid_cmp(&a, &b) == 0);
  }
  friend bool operator!=(const UUID &a, const UUID &b) {
    return (mgos_bt_uuid_cmp(&a, &b) != 0);
  }
};

static_assert(sizeof(UUID) == sizeof(struct shos_bt_uuid),
              "Unexpected size of UUID");

}  // namespace shos::bt

