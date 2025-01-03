/*
 * Copyright (c) 2024 Shelly Group
 * All rights reserved
 */

#pragma once

#include <utility>
#include <vector>

// #include "shos_bt_gap.h"
#include "shos_bt_uuid.hpp"
#include "shos_str.hpp"

namespace shos::bt::gap {

static constexpr size_t kMaxAdvDataLen = 31;

// https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/core/ad_types.yaml
enum class AdvDataType : uint8_t {
  kInvalid = 0x00,
  kFlags = 0x01,
  kService16Incomplete = 0x02,
  kService16 = 0x03,
  kService32Incomplete = 0x04,
  kService32 = 0x05,
  kService128Incomplete = 0x06,
  kService128 = 0x07,
  kShortName = 0x08,
  kFullName = 0x09,
  kPowerLevel = 0x0a,
  kClassOfDevice = 0x0d,
  kSimplePairingHashC192 = 0x0e,
  kSimplePairingRandomizerR192 = 0x0f,
  kDeviceID = 0x10,
  kSecurityManagerOOBFlags = 0x11,
  kPeripheralConnectionIntervalRange = 0x12,
  kServiceSolicitation16 = 0x14,
  kServiceSolicitation128 = 0x15,
  kServiceData16 = 0x16,
  kPublicTargetAddress = 0x17,
  kRandomTargetAddress = 0x18,
  kAppearance = 0x19,
  kAdvInternal = 0x1a,
  kLEDeviceAddress = 0x1b,
  kLERole = 0x1c,
  kSimplePairingHashC256 = 0x1d,
  kSimplePairingRandomizerR256 = 0x1e,
  kServiceSolicitation32 = 0x1f,
  kServiceData32 = 0x20,
  kServiceData128 = 0x21,
  kLESecureConnectionsConfirmationValue = 0x22,
  kLESecureConnectionsRandomValue = 0x23,
  kURL = 0x24,
  kIndoorPositioning = 0x25,
  kTransportDiscoveryData = 0x26,
  kLESupportedFeatures = 0x27,
  kChannelMapUpdateIndication = 0x28,
  kPBADV = 0x29,
  kMeshMessage = 0x2a,
  kMeshBeacon = 0x2b,
  kBIGInfo = 0x2c,
  kBroadcast_Code = 0x2d,
  kResolvableSetIdentifier = 0x2e,
  kLongAdvertisingInterval = 0x2f,
  kBroadcastName = 0x30,
  kEncryptedAdvertisingData = 0x31,
  kPeriodicAdvertisingResponseTimingInformation = 0x32,
  kElectronicShelfLabel = 0x34,
  k3DInformationData = 0x3d,
  kVendorSpecific = 0xff,
};

// Flag value bits for use with AdvDataType::kFlags.
static constexpr uint8_t kFlagDiscoveryLimited = 0x01;
static constexpr uint8_t kFlagDiscoveryGeneral = 0x02;
static constexpr uint8_t kFlagBREDRNotSupported = 0x04;
static constexpr uint8_t kFlagBREDRController = 0x08;
static constexpr uint8_t kFlagBREDRHost = 0x10;

class AdvDataEntry {
 public:
  AdvDataEntry() : v_("", 1) {}
  AdvDataEntry(Str v) : v_(v.empty() ? Str("", 1) : v) {}

  AdvDataType type() const { return static_cast<AdvDataType>(v_[0]); }
  size_t size() const { return (v_.size() - 1); }
  Str data() const { return v_.substr(1); }
  bool IsValid() const { return (type() != AdvDataType::kInvalid); }

  Str value() const { return v_; }
  void AppendTo(std::string &s) const;

 protected:
  using AE = AdvDataEntry;
  friend bool operator==(const AE &a, const AE &b) { return (a.v_ == b.v_); }
  friend bool operator!=(const AE &a, const AE &b) { return (a.v_ != b.v_); }
  friend bool operator<(const AE &a, const AE &b) { return (a.v_ < b.v_); }

  Str v_;
};

class AdvData : public std::vector<AdvDataEntry> {
 public:
  // Splits adv data in and populates the vector.
  // Note: Entries reference the input string.
  Status Parse(Str in, bool append = false);

  // Handy for parsing adv_data and scan_rsp at once.
  Status Parse2(Str in1, Str in2);

  // Returns the full or short name.
  Str GetName() const;

  bool HasService(const UUID &svc) const;

  // Retruns advertised services, if any.
  std::vector<UUID> GetServices() const;

  // Returns the first instance of the data of the given type.
  Str GetDataByType(AdvDataType type) const;

  // Returns all instances of the data of the given type.
  std::vector<Str> GetAllDataByType(AdvDataType type) const;

  // Returns data for a particular service.
  Str GetServiceData(const UUID &svc) const;

  // Returns all service data instances.
  std::vector<std::pair<UUID, Str>> GetAllServiceData() const;

  // Returns vendor-specific data for a particular vendor ID.
  Str GetVendorData(uint16_t vendor_id) const;

  // Returns all instances of kVendorSpecific data.
  std::vector<std::pair<uint16_t, Str>> GetAllVendorData() const;
};

class AdvDataEntryWithStorage : public AdvDataEntry {
 public:
  AdvDataEntryWithStorage(AdvDataType type, Str value);
  AdvDataEntryWithStorage(const AdvDataEntry &e);
  AdvDataEntryWithStorage(const AdvDataEntryWithStorage &other);

  void AppendValue(Str s);
  void AppendValue(const std::string &s);

 private:
  std::string vs_;
};

class AdvDataBuilder : public std::vector<AdvDataEntryWithStorage> {
 public:
  AdvDataBuilder(size_t max_len = kMaxAdvDataLen);

  bool Add(const AdvDataEntry &e);
  bool Add(AdvDataType type, Str value);
  size_t GetTotalSize() const;
  std::string Build() const;

  bool AddFlags(uint8_t flags) {
    return Add(AdvDataType::kFlags, Str(&flags, 1));
  }
  bool AddName(Str name, bool full = true) {
    return Add((full ? AdvDataType::kFullName : AdvDataType::kShortName), name);
  }
  bool AddVendorSpecific(Str value) {
    return Add(AdvDataType::kVendorSpecific, value);
  }
  bool AddService(const UUID &svc);

 private:
  std::string BuildServices() const;

  const size_t max_len_ = 0;
  std::vector<UUID> svcs_;
};

}  // namespace shos::bt::gap
