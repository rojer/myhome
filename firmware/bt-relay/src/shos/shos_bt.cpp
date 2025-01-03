/*
 * Copyright (c) 2021 Deomid "rojer" Ryabkov
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "shos_bt_addr.hpp"
#include "shos_bt_uuid.hpp"

#include <cstring>

// #include "shos_json_utils.hpp"

namespace shos::bt {

Addr::Addr() {
  std::memset(addr, 0, sizeof(addr));
  type = MGOS_BT_ADDR_TYPE_NONE;
}

Addr::Addr(const shos_bt_addr *other) {
  std::memcpy(addr, other->addr, sizeof(addr));
  type = other->type;
}

Addr::Addr(const shos_bt_addr &other) : Addr(&other) {}

Addr::Addr(Str addr_str) : Addr() {
  mgos_bt_addr_from_str(addr_str, this);
}

Addr::Addr(const void *addrv, bool little_endian) {
  const uint8_t *ab = static_cast<const uint8_t *>(addrv);
  if (little_endian) {
    addr[0] = ab[5];
    addr[1] = ab[4];
    addr[2] = ab[3];
    addr[3] = ab[2];
    addr[4] = ab[1];
    addr[5] = ab[0];
  } else {
    addr[0] = ab[0];
    addr[1] = ab[1];
    addr[2] = ab[2];
    addr[3] = ab[3];
    addr[4] = ab[4];
    addr[5] = ab[5];
  }
  type = MGOS_BT_ADDR_TYPE_NONE;
}

std::string Addr::ToString(uint32_t flags) const {
  char buf[MGOS_BT_ADDR_STR_LEN];
  return mgos_bt_addr_to_str(this, flags, buf);
}

UUID::UUID(const void *_uuid, size_t _len) : UUID() {
  switch (_len) {
    case 2: uuid.uuid16 = *static_cast<const uint16_t *>(_uuid); break;
    case 4: uuid.uuid32 = *static_cast<const uint32_t *>(_uuid); break;
    case 16:
      mgos_bt_uuid128_from_bytes(static_cast<const uint8_t *>(_uuid), false,
                                 this);
      break;
    default: return;
  }
  this->len = _len;
}

bool UUID::IsValid() const {
  const size_t len = Len();
  return (len == 2 || len == 4 || len == 16);
}

uint16_t UUID::To16() const {
  return (Len() == 2 ? uuid.uuid16 : 0);
}

uint32_t UUID::To32() const {
  return (Len() == 4 ? uuid.uuid32 : 0);
}

#if 0
std::string UUID::ToBytes() const {
  std::string res;
  switch (Len()) {
    case 2: res.assign(reinterpret_cast<const char *>(&uuid128[12]), 2); break;
    case 4: res.assign(reinterpret_cast<const char *>(&uuid128[12]), 4); break;
    case 16: res.assign(reinterpret_cast<const char *>(&uuid128[0]), 16); break;
  }
  return res;
}

std::string UUID::ToString(uint32_t flags) const {
  char buf[MGOS_BT_UUID_STR_LEN];
  return mgos_bt_uuid_to_str(this, buf);
}
#endif

}  // namespace shos::bt
