/*
 * Copyright (c) 2023 Allterco Robotics
 * All rights reserved
 */

#include "shos_str.hpp"

#include <cstdlib>
#include <cstring>

// #include "shos_base64.h"
// #include "shos_utils.hpp"

namespace shos {

const size_t Str::npos;

Str::Str(const char *s) {
  p = s;
  len = (s ? std::strlen(s) : 0);
}

const char *Str::SafeP() const {
  return (p != nullptr ? p : "");
}

bool Str::operator==(Str other) const {
  if (len != other.len) return false;
  if (len == 0) return true;
  if (p == nullptr || other.p == nullptr) return (p == other.p);
  return (std::memcmp(p, other.p, len) == 0);
}

bool Str::operator==(const char *other) const {
  if (other == nullptr) return (len == 0);
  const char *p1 = p;
  for (size_t len1 = len; len1 > 0; len1--, p1++, other++) {
    if (*other != *p1) return false;
    if (*other == '\0') return false;
  }
  return (*other == '\0');
}

int Str::Cmp(Str other) const {
  const size_t min_len = std::min(len, other.len);
  if (min_len > 0) {
    int res = std::memcmp(p, other.p, min_len);
    if (res != 0) return (res > 0 ? 1 : -1);
  }
  if (len == other.len) return 0;
  return (len < other.len ? -1 : 1);
}

int Str::CaseCmp(Str other) const {
  const size_t min_len = std::min(len, other.len);
  for (size_t i = 0; i < min_len; i++) {
    const int c = std::tolower(p[i]);
    const int other_c = std::tolower(other.p[i]);
    if (c < other_c) return -1;
    if (c > other_c) return 1;
  }
  if (len == other.len) return 0;
  return (len < other.len ? -1 : 1);
}

bool Str::StartsWith(Str prefix) const {
  if (len < prefix.len) return false;
  for (size_t i = 0; i < prefix.len; i++) {
    if (p[i] != prefix[i]) return false;
  }
  return true;
}

static bool StrMatchesPattern(Str s, Str p, bool ignore_case, int depth) {
  if (depth > 10) return false;  // Limit the recursion depth.
  size_t si = 0, pi = 0;
  auto get_ch = [ignore_case](const char *p, size_t i) {
    const char ch = p[i];
    return (ignore_case ? tolower(ch) : ch);
  };
  while (si < s.len && pi < p.len) {
    const char sc = get_ch(s.p, si);
    const char pc = get_ch(p.p, pi);
    switch (pc) {
      case '?': break;
      case '*': {
        pi++;
        if (pi == p.len) return true;
        const char npc = get_ch(p.p, pi);
        const Str sub_pattern = p.substr(pi);
        while (si < s.len) {
          const char nc = get_ch(s.p, si);
          if (nc == npc && StrMatchesPattern(s.substr(si), sub_pattern,
                                             ignore_case, depth + 1)) {
            return true;
          }
          si++;
        }
        return false;
      }
      default:
        if (sc != pc) return false;
    }
    si++;
    pi++;
  }
  return (si == s.len && (pi == p.len || p.substr(pi) == "*"));
}

bool Str::MatchesPattern(Str pattern, bool ignore_case) const {
  return StrMatchesPattern(*this, pattern, ignore_case, 0);
}

bool Str::MatchesPatterns(Str patterns, bool ignore_case) const {
  Str pattern;
  bool split;
  do {
    split = patterns.SplitOn(',', pattern, patterns);
    if (MatchesPattern(pattern, ignore_case)) return true;
  } while (split);
  return false;
}

void Str::ChopLeft(size_t n) {
  len -= n;
  p += n;
}

Str Str::substr(size_t from, size_t count) const {
  if (from >= len) return Str();
  return Str(p + from, std::min(len - from, count));
}

size_t Str::find(char ch, size_t pos) const {
  for (size_t i = pos; i < len; i++) {
    if (p[i] == ch) return i;
  }
  return npos;
}

size_t Str::find(Str substr, size_t pos) const {
  if (pos <= len) {
    Str s(*this);
    if (pos > 0) s.ChopLeft(pos);
    while (s.len >= substr.len) {
      if (s.StartsWith(substr)) return (len - s.len);
      s.ChopLeft(1);
    }
  }
  return npos;
}

size_t Str::rfind(char ch, size_t pos) const {
  if (len == 0) return npos;
  size_t i = std::min(len - 1, pos);
  do {
    if (p[i] == ch) return i;
  } while (i-- > 0);
  return npos;
}

size_t Str::find_first_of(Str chars, size_t pos) const {
  for (; pos < len; pos++) {
    if (chars.find(p[pos]) != npos) return pos;
  }
  return npos;
}

size_t Str::find_first_not_of(Str chars, size_t pos) const {
  for (; pos < len; pos++) {
    if (chars.find(p[pos]) == npos) return pos;
  }
  return npos;
}

size_t Str::strspn(Str accept) const {
  const size_t n = find_first_not_of(accept);
  return (n != npos ? n : len);
}

Str Str::Strspn(Str accept) const {
  return Str(p, strspn(accept));
}

size_t Str::strcspn(Str reject) const {
  const size_t n = find_first_of(reject);
  return (n != npos ? n : len);
}

Str Str::Strcspn(Str reject) const {
  return Str(p, strcspn(reject));
}

bool Str::SplitOn(char sep, Str &before, Str &after) const {
  const size_t i = find(sep);
  if (i == npos) {
    before = *this;
    after.clear();
    return false;
  }
  const Str bf = substr(0, i);
  if (i == 0 || i - 1 < len) {
    after = substr(i + 1, len - i - 1);
  } else {
    after = Str();
  }
  before = bf;
  return true;
}

std::vector<Str> Str::SplitOn(char sep, bool skip_empty) const {
  std::vector<Str> res;
  Str part, rest(*this);
  while (rest.SplitOn(sep, part, rest)) {
    if (!part.empty() || !skip_empty) {
      res.emplace_back(part);
    }
  }
  if (!part.empty() || (!skip_empty && !empty())) {
    res.emplace_back(part);
  }
  return res;
}

Str Str::Strip() {
  Str res(*this);
  while (!res.empty() && std::isspace(res[0])) {
    res.ChopLeft(1);
  }
  while (!res.empty() && std::isspace(res[res.len - 1])) {
    res.ChopRight(1);
  }
  return res;
}

std::string Str::ToString() const {
  return (len > 0 && p != nullptr ? std::string(p, len) : std::string());
}

bool Str::CopyTo(void *vbuf, size_t buf_size, bool nul_terminate) const {
  char *buf = static_cast<char *>(vbuf);
  const size_t to_copy = std::min(len, buf_size);
  std::memcpy(buf, p, to_copy);
  if (!nul_terminate) {
    return (to_copy == len);
  }
  if (to_copy == buf_size) {
    // Best-effort NUL termination.
    if (to_copy > 0) buf[to_copy - 1] = '\0';
    return false;
  }
  buf[to_copy] = '\0';
  return true;
}

static char hexc(uint8_t c, bool upper) {
  if (c < 10) {
    return '0' + c;
  }
  return (upper ? 'A' : 'a') + (c - 10);
}

bool Str::HexEncode(void *out, size_t &out_size, bool upper) const {
  if (out_size < len * 2) return false;
  const char *pi = p;
  char *po = static_cast<char *>(out);
  for (size_t i = 0; i < len; i++) {
    uint8_t c = *pi++;
    *(po++) = hexc(c >> 4, upper);
    *(po++) = hexc(c & 0xf, upper);
  }
  out_size = len * 2;
  return true;
}

bool Str::HexEncode(std::string &out, bool upper) const {
  if (len == 0) {
    out.clear();
    return true;
  }
  size_t size = len * 2;
  out.resize(size);
  if (!HexEncode(const_cast<char *>(out.data()), size, upper)) {
    out.clear();
    return false;
  }
  out.shrink_to_fit();
  return true;
}

std::string Str::ToHexString(bool upper) const {
  std::string res;
  size_t size = len * 2;
  res.resize(size);
  HexEncode(const_cast<char *>(res.data()), size, upper);
  return res;
}

static bool HexDigit(char c, uint8_t &v) {
  if (c >= '0' && c <= '9') {
    v = c - '0';
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    v = 10 + (c - 'a');
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    v = 10 + (c - 'A');
    return true;
  }
  return false;
}

bool Str::HexDecode(void *out, size_t &out_size) const {
  if (len % 2 != 0) return false;
  if (out_size < len / 2) return false;
  const char *ip = p, *ep = p + len;
  uint8_t *op = static_cast<uint8_t *>(out);
  while (ip != ep) {
    uint8_t v1, v2;
    if (!HexDigit(*ip++, v1)) return false;
    if (!HexDigit(*ip++, v2)) return false;
    *op++ = ((v1 << 4) | v2);
  }
  out_size = len / 2;
  return true;
}

bool Str::HexDecode(std::string &out) const {
  size_t size = len / 2;
  out.resize(size);
  if (!HexDecode(const_cast<char *>(out.data()), size)) {
    out.clear();
    return false;
  }
  out.shrink_to_fit();
  return true;
}

StatusOr<std::string> Str::HexDecode() const {
  std::string res;
  if (!HexDecode(res)) return Status::INVALID_ARGUMENT();
  return res;
}

bool Str::HexDecodeInPlace() {
  return HexDecode(const_cast<char *>(p), len);
}

#if 0
bool Str::Base64Encode(void *out, size_t &out_size) const {
  return shos_base64_encode(p, len, out, &out_size);
}

bool Str::Base64Encode(std::string &out) const {
  if (len == 0) {
    out.clear();
    return true;
  }
  size_t need_size = shos_base64_get_encoded_size(len);
  out.resize(need_size);
  if (!shos_base64_encode(p, len, (void *) out.data(), &need_size)) {
    out.clear();
  }
  out.shrink_to_fit();
  return true;
}
#endif

std::string Str::ToBase64String() const {
  std::string out;
  Base64Encode(out);
  return out;
}

#if 0
bool Str::Base64Decode(void *out, size_t &out_size) const {
  return shos_base64_decode(p, len, out, &out_size);
}

bool Str::Base64DecodeInPlace() {
  return Base64Decode((void *) p, len);
}

bool Str::Base64Decode(std::string &out) const {
  if (len == 0) {
    out.clear();
    return true;
  }
  out.resize(shos_base64_get_decoded_size(p, len));
  size_t out_size = out.size();
  if (!Base64Decode((void *) out.data(), out_size)) return false;
  out.resize(out_size);
  return true;
}

StatusOr<std::string> Str::Base64Decode() const {
  std::string out;
  if (!Base64Decode(out)) {
    return Status::INVALID_ARGUMENT();
  }
  return out;
}
#endif

static int GetVal(char c, int base) {
  switch (base) {
    case 2:
      if (c >= '0' && c <= '1') {
        return c - '0';
      }
      break;
    case 8:
      if (c >= '0' && c <= '7') {
        return c - '0';
      }
      break;
    case 10:
      if (c >= '0' && c <= '9') {
        return c - '0';
      }
      break;
    case 16:
      if (c >= '0' && c <= '9') {
        return c - '0';
      } else if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
      } else if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
      }
      break;
  }
  return -1;
}

template <class T>
StatusOr<T> ToUInt(Str s, int base, size_t width, T max) {
  if (s.len == 0) {
    return Status::INVALID_ARGUMENT();
  }
  if (base == 0) {
    base = 10;
    if (s.len > 2) {
      switch (s[1]) {
        case 'b':
          base = 2;
          s.ChopLeft(2);
          break;
        case 'o':
          base = 8;
          s.ChopLeft(2);
          break;
        case 'x':
          base = 16;
          s.ChopLeft(2);
          break;
      }
    }
  }
  while (s.len > 0 && s[0] == '0') {
    s.ChopLeft(1);
  }
  static const uint8_t kMaxUIntLens[4][4] = {
      {8, 16, 32, 64},  // base 2
      {3, 6, 11, 22},   // base 8
      {3, 5, 10, 20},   // base 10
      {2, 4, 8, 16},    // base 16
  };
  uint8_t max_len;
  switch (base) {
    case 2: max_len = kMaxUIntLens[0][width]; break;
    case 8: max_len = kMaxUIntLens[1][width]; break;
    case 10: max_len = kMaxUIntLens[2][width]; break;
    case 16: max_len = kMaxUIntLens[3][width]; break;
    default: return Status::INVALID_ARGUMENT();
  }
  if (s.len > max_len) {
    return Status::INVALID_ARGUMENT();
  }
  T res = 0, mult = 1;
  for (int i = s.len - 1; i >= 0; i--, mult *= base) {
    int val = GetVal(s[i], base);
    if (val < 0) {
      return Status::INVALID_ARGUMENT();
    }
    for (; val > 0; val--) {
      const T old_res = res;
      res += mult;
      if (res < old_res || res > max) {
        return Status::INVALID_ARGUMENT();
      }
    }
  }
  return res;
}

StatusOr<uint8_t> Str::ToUInt8(int base) const {
  return ToUInt<uint8_t>(*this, base, 0, 0xff);
}

StatusOr<uint16_t> Str::ToUInt16(int base) const {
  return ToUInt<uint16_t>(*this, base, 1, 0xffff);
}

StatusOr<uint32_t> Str::ToUInt32(int base) const {
  return ToUInt<uint32_t>(*this, base, 2, 0xffffffff);
}

StatusOr<uint64_t> Str::ToUInt64(int base) const {
  return ToUInt<uint64_t>(*this, base, 3, 0xffffffffffffffff);
}

template <class T, class UT>
StatusOr<T> ToInt(Str s, size_t width, UT max) {
  bool neg = false;
  if (!s.empty() && s[0] == '-') {
    neg = true;
    s.ChopLeft(1);
  }
  const auto uvs = ToUInt<UT>(s, 10, width, max);
  if (!uvs.ok()) {
    return uvs.status();
  }
  const UT uv = uvs.ValueOrDie();
  if (uv == 0 && neg) {
    return Status::INVALID_ARGUMENT();
  } else if (uv == max) {
    if (neg) {
      return -max;
    } else {
      return Status::INVALID_ARGUMENT();
    }
  }
  const T v = uv;
  return (neg ? -v : v);
}

StatusOr<int8_t> Str::ToInt8() const {
  return ToInt<int8_t, uint8_t>(*this, 0, 0x80);
}

StatusOr<int16_t> Str::ToInt16() const {
  return ToInt<int16_t, uint16_t>(*this, 1, 0x8000);
}

StatusOr<int32_t> Str::ToInt32() const {
  return ToInt<int32_t, uint32_t>(*this, 2, 0x80000000);
}

StatusOr<int64_t> Str::ToInt64() const {
  return ToInt<int64_t, uint64_t>(*this, 3, 0x8000000000000000);
}

// mg_str API impl.
extern "C" {

struct shos_str shos_mk_str(const char *s) {
  return Str(s);
}

struct shos_str shos_mk_str_n(const char *s, size_t len) {
  return Str(s, len);
}

int shos_vcmp(const struct shos_str *str1, const char *str2) {
  const Str *mstr1 = static_cast<const Str *>(str1);
  return mstr1->Cmp(str2);
}

int shos_vcasecmp(const struct shos_str *str1, const char *str2) {
  const Str *mstr1 = static_cast<const Str *>(str1);
  return mstr1->CaseCmp(str2);
}

static struct shos_str shos_strdup_plus(const struct shos_str s, size_t plus) {
  struct shos_str res = SHOS_NULL_STR;
  const size_t len = s.len;
  if (len != 0) {
    char *p = static_cast<char *>(std::malloc(len + plus));
    if (p != nullptr) {
      std::memcpy(p, s.p, len);
      res.p = p;
      res.len = len;
    }
  }
  return res;
}

struct shos_str shos_strdup(const struct shos_str s) {
  return shos_strdup_plus(s, 0);
}

struct shos_str shos_strdup_nul(const struct shos_str s) {
  struct shos_str res = shos_strdup_plus(s, 1);
  if (res.p != nullptr) {
    const_cast<char *>(res.p)[res.len] = '\0';
  }
  return res;
}

const char *shos_strchr(const struct shos_str s, int c) {
  const Str ms(s);
  const size_t i = ms.find(c);
  if (i == Str::npos) return NULL;
  return (s.p + i);
}

int shos_strcmp(const struct shos_str str1, const struct shos_str str2) {
  return Str(str1).Cmp(str2);
}

int shos_strncmp(const struct shos_str str1, const struct shos_str str2,
                 size_t n) {
  const Str mstr1(str1.p, std::min(str1.len, n));
  const Str mstr2(str2.p, std::min(str2.len, n));
  return mstr1.Cmp(mstr2);
}

int shos_strcasecmp(const struct shos_str str1, const struct shos_str str2) {
  return Str(str1).CaseCmp(str2);
}

void shos_strfree(struct shos_str *s) {
  s->len = 0;
  std::free(const_cast<char *>(s->p));
  s->p = nullptr;
}

const char *shos_strstr(const struct shos_str haystack,
                        const struct shos_str needle) {
  const Str ms(haystack);
  const auto i = ms.find(needle);
  if (i == Str::npos) return NULL;
  return (haystack.p + i);
}

struct shos_str shos_strstrip(struct shos_str s) {
  Str ms(s);
  while (!ms.empty() && isspace(ms[0])) {
    ms.ChopLeft(1);
  }
  while (!ms.empty() && isspace(ms[ms.len - 1])) {
    ms.len--;
  }
  return ms;
}

int shos_str_starts_with(struct shos_str s, struct shos_str prefix) {
  return Str(s).StartsWith(prefix);
}

struct shos_str shos_next_comma_list_entry_n(struct shos_str list,
                                             struct shos_str *val,
                                             struct shos_str *eq_val) {
  Str entry, rest;
  Str(list).SplitOn(',', entry, rest);
  if (!entry.empty()) {
    if (eq_val != nullptr) {
      Str key, value;
      if (entry.SplitOn('=', key, value)) {
        *eq_val = value;
      } else {
        eq_val->p = nullptr;
        eq_val->len = 0;
      }
      *val = key;
    } else {
      *val = entry;
    }
    if (rest.empty()) {
      rest.p = entry.p + entry.len;
    }
  }
  return rest;
}

bool shos_str_matches_pattern(struct shos_str str, struct shos_str pattern,
                              bool ignore_case) {
  return Str(str).MatchesPattern(pattern, ignore_case);
}

bool shos_str_matches_patterns(struct shos_str str, struct shos_str patterns,
                               bool ignore_case) {
  return Str(str).MatchesPatterns(patterns, ignore_case);
}

}  // extern "C"

}  // namespace shos
