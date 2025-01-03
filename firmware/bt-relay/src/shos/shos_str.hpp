/*
 * Copyright (c) 2023 Allterco Robotics
 * All rights reserved
 */

#pragma once

#include "common/mg_str.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// #include "shos_json.h"
// #include "shos_scoped_ptr.hpp"
#include "common/util/statusor.h"
#include "mgos_json_utils.hpp"
#include "mgos_utils.hpp"

namespace shos {

using mgos::Errorf;
using mgos::SPrintf;
using mgos::StatusOr;

#define shos_str mg_str
#define shos_strdup mg_strdup
#define SHOS_NULL_STR MG_NULL_STR
class Status : public mgos::Status {
 public:
  Status() : mgos::Status() {}
  Status(int code, const char *msg) : mgos::Status(code, msg) {}
  Status(const mgos::Status &other) : mgos::Status(other) {}
  static Status OK() { return Status(); }
  static Status NOT_FOUND() { return Status(STATUS_NOT_FOUND, "Not found"); }
  static Status INVALID_ARGUMENT() {
    return Status(STATUS_INVALID_ARGUMENT, "Invalid argument");
  }
};

// Our variant of StringPiece, string_view, or whatever.
// A pointer + length, small enough to be passed by value.
// Memory layout is compatible with the legacy struct shos_str.
struct Str : public shos_str {
  Str() : Str(nullptr, 0) {}
  Str(const void *p_, size_t len_) {
    p = static_cast<const char *>(p_);
    len = len_;
  }
  ~Str() { clear(); }
  Str(const char *s);
  // Str(const ScopedCharPtr &sp) : Str(sp.get()) {}

  Str &operator=(const Str &other) {
    p = other.p;
    len = other.len;
    return *this;
  }
  Str(Str &&other) {
    *this = other;
    other.clear();
  }
  Str &operator=(Str &&other) {
    *this = other;
    other.clear();
    return *this;
  }
  Str(const Str &other) : Str(other.p, other.len) {}
  Str(const struct shos_str &other) : Str(other.p, other.len) {}
  // Str(const struct json_token &tok) : Str(tok.ptr, tok.len) {}
  Str(const std::string &s) : Str(s.data(), s.length()) {}
  Str(const std::vector<unsigned char> &v) : Str(v.data(), v.size()) {}
  Str(const std::vector<char> &v) : Str(v.data(), v.size()) {}

  // Prevent construction from temporary std::string.
  // Pull that gun away from the foot.
  Str(const std::string &&s) = delete;  // No construction from temporaries.

  // Different flavors for the pointer.
  const void *vp() const { return static_cast<const void *>(p); }
  const unsigned char *up() const { return (const unsigned char *) p; }
  const char *SafeP() const;  // Returns empty string when p is null.

  // std::string-like interface
  inline const char *data() const { return p; }
  inline size_t length() const { return len; }
  inline size_t size() const { return len; }
  inline bool empty() const { return (len == 0); }
  inline void clear() {
    len = 0;
    p = nullptr;
  }
  static constexpr size_t npos = std::string::npos;
  size_t find(char ch, size_t pos = 0) const;
  size_t find(Str substr, size_t pos = 0) const;
  size_t rfind(char ch, size_t pos = npos) const;
  size_t rfind(Str substr, size_t pos = npos) const;
  size_t find_first_of(Str chars, size_t pos = 0) const;
  size_t find_first_not_of(Str chars, size_t pos = 0) const;
  Str substr(size_t pos = 0, size_t count = npos) const;

  // Comparisons.
  int Cmp(Str other) const;
  int CaseCmp(Str other) const;
  bool operator==(Str other) const;
  bool operator==(const char *other) const;
  bool operator!=(Str other) const { return !(*this == other); }
  bool operator!=(const char *other) const { return !(*this == other); }
  bool operator<(Str other) const { return Cmp(other) < 0; }
  bool operator>(Str other) const { return Cmp(other) > 0; }

  // Returns true if this string matches the given pattern.
  // Pattern supports '?' for single and '*' for zero or more characters.
  bool MatchesPattern(Str pattern, bool ignore_case = false) const;

  // Returns true if this string matches one of the given patterns.
  // Patterns are delimited by commas.
  bool MatchesPatterns(Str patterns, bool ignore_case = false) const;

  // Array-like read access.
  char operator[](size_t i) const { return p[i]; }

  // Utility functions.
  bool StartsWith(Str prefix) const;

  size_t strspn(Str accept) const;
  Str Strspn(Str accept) const;
  size_t strcspn(Str reject) const;
  Str Strcspn(Str reject) const;

  void ChopLeft(size_t n);
  void ChopRight(size_t n) { len -= n; }

  // Split the string on the given separator into two parts.
  // Returns true if the separator was found, false of not.
  // If false is returned, before contains the entire string
  // and after is empty.
  // It is ok to use the object itself as before or after.
  bool SplitOn(char sep, Str &before, Str &after) const;

  // A variant that returns all the resulting parts at once.
  std::vector<Str> SplitOn(char sep, bool skip_empty = false) const;

  // Return Str with whitespace stripped on both ends.
  Str Strip();

  // Replaces s with the contents of this.
  void CopyTo(std::string &s) const { s.assign(SafeP(), len); }

  // Copies up to buf_size bytes to buf, optionally NUL-terminating.
  // Returns true if fits.
  bool CopyTo(void *buf, size_t buf_size, bool nul_terminate = false) const;

  // Appends contents to s.
  void AppendTo(std::string &s) const { s.append(SafeP(), len); }

  // Conversions
  std::string ToString() const;

  // Hex.
  bool HexEncode(void *out, size_t &out_size, bool upper = false) const;
  bool HexEncode(std::string &out, bool upper = false) const;
  std::string ToHexString(bool upper = false) const;

  bool HexDecodeInPlace();
  bool HexDecode(std::string &out) const;
  bool HexDecode(void *out, size_t &out_size) const;
  StatusOr<std::string> HexDecode() const;

  // Base64.
  bool Base64Encode(void *out, size_t &out_size) const;
  bool Base64Encode(std::string &out) const;
  std::string ToBase64String() const;

  bool Base64DecodeInPlace();
  bool Base64Decode(std::string &out) const;
  bool Base64Decode(void *out, size_t &out_size) const;
  StatusOr<std::string> Base64Decode() const;

  // Unsigned conversions.
  // Base 2, 8, 10 and 16 conversions are supported.
  // If base is 0, it is determined by the prefix: 0b, 0o, 0x.
  // Note that octal conversions require 0o prefix, leading zero
  // does not result in base 8 conversion (010 is 10 decimal, not 8).
  StatusOr<uint8_t> ToUInt8(int base = 10) const;
  StatusOr<uint16_t> ToUInt16(int base = 10) const;
  StatusOr<uint32_t> ToUInt32(int base = 10) const;
  StatusOr<uint64_t> ToUInt64(int base = 10) const;

  // Signed conversions. Only base 10 is supported.
  StatusOr<int8_t> ToInt8() const;
  StatusOr<int16_t> ToInt16() const;
  StatusOr<int32_t> ToInt32() const;
  StatusOr<int64_t> ToInt64() const;

  // Iterator interface.
  class Iterator {
   public:
    Iterator(const Str &s, size_t start) : s_(s), i_(start) {}
    char operator*() { return s_[i_]; }
    bool operator==(const Iterator &other) { return other.i_ == i_; }
    bool operator!=(const Iterator &other) { return !(*this == other); }
    void operator++() { i_++; }

   private:
    const Str &s_;
    size_t i_;
  };

  Iterator begin() const { return Iterator(*this, 0); }
  Iterator end() const { return Iterator(*this, len); }
};

// Useful macro for using in printf.
#define SHOSTRF(s) (unsigned) (s).len, (s).SafeP()

static_assert(sizeof(Str) == sizeof(struct shos_str),
              "shos::Str must be the same size as shos_str");

}  // namespace shos
