/*
 * Copyright (c) 2023 Shelly Group
 * All rights reserved
 */

#include "shos_bt_gap_adv.hpp"

#include "mgos.hpp"

namespace shos::bt::gap {

struct AdvDataEntryHeader {
  uint8_t len;
  AdvDataType type;
} SHOS_PACKED;

Status AdvData::Parse(Str in, bool append) {
  if (!append) clear();
  while (!in.empty()) {
    if (in.size() < 2) return Status::INVALID_ARGUMENT();
    const auto &h = *reinterpret_cast<const AdvDataEntryHeader *>(in.data());
    in.ChopLeft(1);
    if (h.len == 0 || h.len > in.size()) return Status::INVALID_ARGUMENT();
    emplace_back(in.substr(0, h.len));
    in.ChopLeft(h.len);
  }
  return Status::OK();
}

Status AdvData::Parse2(Str in1, Str in2) {
  Status st = Parse(in1, false);
  if (st.ok()) st = Parse(in2, true);
  return st;
}

Str AdvData::GetDataByType(AdvDataType type) const {
  for (const AdvDataEntry &e : *this) {
    if (e.type() == type) return e.data();
  }
  return Str();
}

std::vector<Str> AdvData::GetAllDataByType(AdvDataType type) const {
  std::vector<Str> res;
  for (const AdvDataEntry &e : *this) {
    if (e.type() == type) res.emplace_back(e.data());
  }
  return res;
}

Str AdvData::GetServiceData(const UUID &svc) const {
  const size_t uuid_len = svc.Len();
  AdvDataType data_type;
  switch (uuid_len) {
    case 2: data_type = AdvDataType::kServiceData16; break;
    case 4: data_type = AdvDataType::kServiceData32; break;
    case 16: data_type = AdvDataType::kServiceData128; break;
    default: return Str();
  }
  for (const AdvDataEntry &e : *this) {
    if (e.type() != data_type) continue;
    if (e.data().size() < uuid_len) continue;
    if (UUID(e.data().data(), uuid_len) != svc) continue;
    return e.data().substr(uuid_len);
  }
  return Str();
}

std::vector<std::pair<UUID, Str>> AdvData::GetAllServiceData() const {
  std::vector<std::pair<UUID, Str>> res;
  for (const AdvDataEntry &e : *this) {
    size_t uuid_len;
    switch (e.type()) {
      case AdvDataType::kServiceData16: uuid_len = 2; break;
      case AdvDataType::kServiceData32: uuid_len = 4; break;
      case AdvDataType::kServiceData128: uuid_len = 16; break;
      default: continue;
    }
    if (e.data().size() < uuid_len) continue;
    shos::bt::UUID uuid(e.data().data(), uuid_len);
    if (!uuid.IsValid()) continue;
    res.emplace_back(std::make_pair(uuid, e.data().substr(uuid_len)));
  }
  res.shrink_to_fit();
  return res;
}

Str AdvData::GetName() const {
  for (const AdvDataEntry &e : *this) {
    if (e.type() == AdvDataType::kFullName) return e.data();
  }
  for (const AdvDataEntry &e : *this) {
    if (e.type() == AdvDataType::kShortName) return e.data();
  }
  return Str();
}

bool AdvData::HasService(const UUID &svc) const {
  bool res = false;
  const auto parse_uuids = [&svc, &res](Str data, size_t uuid_len) {
    while (data.size() >= uuid_len) {
      if (UUID(data.data(), uuid_len) == svc) {
        res = true;
        break;
      }
      data.ChopLeft(uuid_len);
    }
  };
  for (const AdvDataEntry &e : *this) {
    switch (e.type()) {
      case AdvDataType::kService16:
      case AdvDataType::kService16Incomplete: parse_uuids(e.data(), 2); break;
      case AdvDataType::kService32:
      case AdvDataType::kService32Incomplete: parse_uuids(e.data(), 4); break;
      case AdvDataType::kService128:
      case AdvDataType::kService128Incomplete: parse_uuids(e.data(), 16); break;
      default: break;
    }
    if (res) break;
  }
  return res;
}

std::vector<UUID> AdvData::GetServices() const {
  std::vector<UUID> res;
  const auto parse_uuids = [&res](Str data, size_t uuid_len) {
    while (data.size() >= uuid_len) {
      res.emplace_back(UUID(data.data(), uuid_len));
      data.ChopLeft(uuid_len);
    }
  };
  for (const AdvDataEntry &e : *this) {
    switch (e.type()) {
      case AdvDataType::kService16:
      case AdvDataType::kService16Incomplete: parse_uuids(e.data(), 2); break;
      case AdvDataType::kService32:
      case AdvDataType::kService32Incomplete: parse_uuids(e.data(), 4); break;
      case AdvDataType::kService128:
      case AdvDataType::kService128Incomplete: parse_uuids(e.data(), 16); break;
      default: break;
    }
  }
  return res;
}

Str AdvData::GetVendorData(uint16_t vendor_id) const {
  Str res;
  for (const AdvDataEntry &e : *this) {
    if (e.type() != AdvDataType::kVendorSpecific) continue;
    if (e.size() < 2) continue;
    uint16_t vid;
    e.data().substr(0, 2).CopyTo(&vid, 2);
    if (vid == vendor_id) {
      res = e.data().substr(2);
      break;
    }
  }
  return res;
}

std::vector<std::pair<uint16_t, Str>> AdvData::GetAllVendorData() const {
  std::vector<std::pair<uint16_t, Str>> res;
  for (const AdvDataEntry &e : *this) {
    if (e.type() != AdvDataType::kVendorSpecific) continue;
    if (e.size() < 2) continue;
    uint16_t vid;
    e.data().substr(0, 2).CopyTo(&vid, 2);
    res.emplace_back(std::make_pair(vid, e.data().substr(2)));
  }
  return res;
}

void AdvDataEntry::AppendTo(std::string &s) const {
  s.append(1, static_cast<char>(v_.size()));
  v_.AppendTo(s);
}

AdvDataEntryWithStorage::AdvDataEntryWithStorage(AdvDataType type, Str value) {
  const uint8_t len_type[1] = {static_cast<uint8_t>(type)};
  Str(len_type, 1).AppendTo(vs_);
  value.AppendTo(vs_);
  v_ = vs_;
}

AdvDataEntryWithStorage::AdvDataEntryWithStorage(const AdvDataEntry &e)
    : AdvDataEntryWithStorage(e.type(), e.data()) {}

AdvDataEntryWithStorage::AdvDataEntryWithStorage(
    const AdvDataEntryWithStorage &other)
    : AdvDataEntry() {
  vs_ = other.vs_;
  v_ = vs_;
}

void AdvDataEntryWithStorage::AppendValue(Str s) {
  s.AppendTo(vs_);
  v_ = vs_;
}

void AdvDataEntryWithStorage::AppendValue(const std::string &s) {
  vs_.append(s);
  v_ = vs_;
}

AdvDataBuilder::AdvDataBuilder(size_t max_len) : max_len_(max_len) {}

bool AdvDataBuilder::Add(const AdvDataEntry &e) {
  if (GetTotalSize() + e.value().size() >= max_len_) return false;
  this->emplace_back(e);
  return true;
}

bool AdvDataBuilder::Add(AdvDataType type, Str value) {
  return Add(AdvDataEntryWithStorage(type, value));
}

bool AdvDataBuilder::AddService(const UUID &svc) {
  for (const UUID &svc2 : svcs_) {
    if (svc2 == svc) return true;
  }
  svcs_.emplace_back(svc);
  if (GetTotalSize() > max_len_) {
    svcs_.erase(svcs_.end() - 1);
    return false;
  }
  return true;
}

size_t AdvDataBuilder::GetTotalSize() const {
  size_t total_size = 0;
  for (const auto &e : *this) {
    total_size += 1 + e.value().size();
  }
  return total_size + BuildServices().size();
}

std::string AdvDataBuilder::Build() const {
  std::string res;
  for (const auto &e : *this) {
    e.AppendTo(res);
  }
  res.append(BuildServices());
  res.shrink_to_fit();
  return res;
}

std::string AdvDataBuilder::BuildServices() const {
  std::string res;
  AdvDataEntryWithStorage res16(AdvDataType::kService16Incomplete, nullptr);
  AdvDataEntryWithStorage res32(AdvDataType::kService32Incomplete, nullptr);
  AdvDataEntryWithStorage res128(AdvDataType::kService128Incomplete, nullptr);
  for (const UUID &svc : svcs_) {
    switch (svc.Len()) {
      case 2: res16.AppendValue(svc.ToBytes()); break;
      case 4: res32.AppendValue(svc.ToBytes()); break;
      case 16: res128.AppendValue(svc.ToBytes()); break;
    }
  }
  if (!res16.data().empty()) res16.AppendTo(res);
  if (!res32.data().empty()) res32.AppendTo(res);
  if (!res128.data().empty()) res128.AppendTo(res);
  return res;
}

}  // namespace shos::bt::gap
