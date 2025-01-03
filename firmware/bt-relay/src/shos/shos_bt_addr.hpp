/*
 * Copyright (c) 2024 Shelly Group
 * All rights reserved
 */

#pragma once

#include <endian.h>

#include "mgos_bt.h"
#include "shos_str.hpp"

#include "nimble/ble.h"

namespace shos::bt {

#define shos_bt_addr mgos_bt_addr

struct Addr : public shos_bt_addr {
  Addr();
  Addr(const shos_bt_addr *other);
  Addr(const shos_bt_addr &other);
  Addr(const void *addr, bool little_endian);
  Addr(Str addr_str);  // Hex representation.

  Addr(const ble_addr_t &addr);
  void ToNimBLE(ble_addr_t &addr) const;

  bool IsZero() const { return mgos_bt_addr_is_zero(this); }
  bool IsValid() const { return !IsZero(); }
  static constexpr uint32_t kStringUpper = 1;
  static constexpr uint32_t kStringType = 2;
  static constexpr uint32_t kStringNoColons = 4;
  std::string ToString(uint32_t flags = kStringType) const;

  bool MatchesIRK(Str irk) const;

  friend bool operator<(const Addr &a, const Addr &b) {
    return (mgos_bt_addr_cmp(&a, &b) < 0);
  }
  friend bool operator==(const Addr &a, const Addr &b) {
    return (mgos_bt_addr_cmp(&a, &b) == 0);
  }
  friend bool operator!=(const Addr &a, const Addr &b) {
    return (mgos_bt_addr_cmp(&a, &b) != 0);
  }
};

static_assert(sizeof(Addr) == sizeof(struct shos_bt_addr),
              "Unexpected size of Addr");

}  // namespace shos::bt
