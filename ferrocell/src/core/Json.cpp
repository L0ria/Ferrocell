// =============================================================================
//  Json — implementation (M5, issue #6). Pure C++17, no Arduino deps.
//
//  A small, strict-enough JSON parser (objects, arrays, strings, numbers,
//  booleans, null) + a compact serializer. No external dependencies, so it
//  builds on the host (x86, `g++ -std=c++17`) and in the firmware (xtensa).
// =============================================================================
#include "Json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace ferrocell {
namespace json {

// --- Value factories --------------------------------------------------------
Value Value::makeNull() { Value v; v.type = Type::Null; return v; }
Value Value::makeBool(bool b) { Value v; v.type = Type::Bool; v.boolean = b; return v; }
Value Value::makeNumber(double n) { Value v; v.type = Type::Number; v.number = n; return v; }
Value Value::makeNumber(int n) { return makeNumber(static_cast<double>(n)); }
Value Value::makeString(const std::string& s) { Value v; v.type = Type::String; v.string = s; return v; }
Value Value::makeArray() { Value v; v.type = Type::Array; return v; }
Value Value::makeObject() { Value v; v.type = Type::Object; return v; }

const Value* Value::find(const std::string& key) const {
  if (type != Type::Object) return nullptr;
  for (const auto& kv : object) {
    if (kv.first == key) return &kv.second;
  }
  return nullptr;
}

bool Value::getBool(const std::string& key, bool def) const {
  const Value* v = find(key);
  return v ? v->asBool(def) : def;
}
double Value::getNumber(const std::string& key, double def) const {
  const Value* v = find(key);
  return v ? v->asNumber(def) : def;
}
std::string Value::getString(const std::string& key, const std::string& def) const {
  const Value* v = find(key);
  return v ? v->asString(def) : def;
}

void Value::set(const std::string& key, Value v) {
  if (type != Type::Object) {
    type = Type::Object;
    object.clear();
  }
  for (auto& kv : object) {
    if (kv.first == key) {
      kv.second = std::move(v);
      return;
    }
  }
  object.emplace_back(key, std::move(v));
}

bool Value::asBool(bool def) const {
  if (type == Type::Bool) return boolean;
  if (type == Type::Number) return number != 0.0;
  return def;
}
double Value::asNumber(double def) const {
  if (type == Type::Number) return number;
  if (type == Type::Bool) return boolean ? 1.0 : 0.0;
  return def;
}
std::string Value::asString(const std::string& def) const {
  if (type == Type::String) return string;
  return def;
}

// --- escaping / serialization ----------------------------------------------
std::string escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

namespace {
void appendNumber(std::string& out, double n) {
  if (std::isnan(n) || std::isinf(n)) {
    out += "null";
    return;
  }
  // Integral values print without a fractional part (e.g. 210, 30, 1).
  if (n == std::floor(n) && std::fabs(n) < 1e15) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(n));
    out += buf;
  } else {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.10g", n);
    out += buf;
  }
}

void dumpInto(std::string& out, const Value& v) {
  switch (v.type) {
    case Value::Type::Null: out += "null"; break;
    case Value::Type::Bool: out += v.boolean ? "true" : "false"; break;
    case Value::Type::Number: appendNumber(out, v.number); break;
    case Value::Type::String:
      out += '"';
      out += escape(v.string);
      out += '"';
      break;
    case Value::Type::Array: {
      out += '[';
      bool first = true;
      for (const auto& e : v.array) {
        if (!first) out += ',';
        first = false;
        dumpInto(out, e);
      }
      out += ']';
      break;
    }
    case Value::Type::Object: {
      out += '{';
      bool first = true;
      for (const auto& kv : v.object) {
        if (!first) out += ',';
        first = false;
        out += '"';
        out += escape(kv.first);
        out += "\":";
        dumpInto(out, kv.second);
      }
      out += '}';
      break;
    }
  }
}
}  // namespace

std::string dump(const Value& v) {
  std::string out;
  dumpInto(out, v);
  return out;
}

// --- parser ----------------------------------------------------------------
namespace {

struct Parser {
  const std::string& s;
  std::size_t i = 0;
  std::string* err;

  explicit Parser(const std::string& src, std::string* e) : s(src), err(e) {}

  void fail(const std::string& msg) {
    if (err) {
      *err = msg + " at index " + std::to_string(i);
    }
    throw std::runtime_error(msg);
  }

  void skipWs() {
    while (i < s.size()) {
      char c = s[i];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i;
      else break;
    }
  }

  bool eof() { skipWs(); return i >= s.size(); }
  char peek() { skipWs(); return i < s.size() ? s[i] : '\0'; }
  char next() { skipWs(); if (i >= s.size()) fail("unexpected end"); return s[i++]; }
  bool consume(char c) { skipWs(); if (i < s.size() && s[i] == c) { ++i; return true; } return false; }

  void expect(char c) {
    char g = next();
    if (g != c) {
      std::string m = "expected '";
      m += c;
      m += "'";
      fail(m);
    }
  }

  Value parseValue() {
    char c = peek();
    if (c == '{') return parseObject();
    if (c == '[') return parseArray();
    if (c == '"') return Value::makeString(parseString());
    if (c == 't' || c == 'f') return parseBool();
    if (c == 'n') return parseNull();
    return parseNumber();
  }

  Value parseObject() {
    Value v = Value::makeObject();
    expect('{');
    if (consume('}')) return v;
    while (true) {
      std::string key = parseString();
      expect(':');
      Value val = parseValue();
      v.object.emplace_back(std::move(key), std::move(val));
      if (consume(',')) continue;
      expect('}');
      return v;
    }
  }

  Value parseArray() {
    Value v = Value::makeArray();
    expect('[');
    if (consume(']')) return v;
    while (true) {
      v.array.push_back(parseValue());
      if (consume(',')) continue;
      expect(']');
      return v;
    }
  }

  std::string parseString() {
    expect('"');
    std::string out;
    while (true) {
      if (i >= s.size()) fail("unterminated string");
      char c = s[i++];
      if (c == '"') break;
      if (c == '\\') {
        if (i >= s.size()) fail("bad escape");
        char e = s[i++];
        switch (e) {
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case '/': out += '/'; break;
          case 'b': out += '\b'; break;
          case 'f': out += '\f'; break;
          case 'n': out += '\n'; break;
          case 'r': out += '\r'; break;
          case 't': out += '\t'; break;
          case 'u': {
            if (i + 4 > s.size()) fail("bad \\u escape");
            unsigned int cp = 0;
            for (int k = 0; k < 4; ++k) {
              char h = s[i + k];
              cp <<= 4;
              if (h >= '0' && h <= '9') cp |= (h - '0');
              else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
              else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
              else fail("bad hex digit");
            }
            i += 4;
            // Encode as UTF-8 (BMP only; surrogate pairs are not required by
            // the API but we emit the code point as UTF-8 for safety).
            if (cp < 0x80) {
              out += static_cast<char>(cp);
            } else if (cp < 0x800) {
              out += static_cast<char>(0xC0 | (cp >> 6));
              out += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
              out += static_cast<char>(0xE0 | (cp >> 12));
              out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
              out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            break;
          }
          default: fail("unknown escape");
        }
      } else {
        out += c;
      }
    }
    return out;
  }

  Value parseBool() {
    if (s.compare(i, 4, "true") == 0) { i += 4; return Value::makeBool(true); }
    if (s.compare(i, 5, "false") == 0) { i += 5; return Value::makeBool(false); }
    fail("invalid literal");
    return Value::makeNull();
  }

  Value parseNull() {
    if (s.compare(i, 4, "null") == 0) { i += 4; return Value::makeNull(); }
    fail("invalid literal");
    return Value::makeNull();
  }

  Value parseNumber() {
    skipWs();
    std::size_t start = i;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
    bool any = false;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; any = true; }
    if (i < s.size() && s[i] == '.') {
      ++i;
      while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; any = true; }
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
      ++i;
      if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
      while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    }
    if (!any) fail("invalid number");
    std::string tok = s.substr(start, i - start);
    return Value::makeNumber(std::strtod(tok.c_str(), nullptr));
  }
};

}  // namespace

bool parse(const std::string& text, Value& out, std::string* err) {
  try {
    Parser p(text, err);
    out = p.parseValue();
    if (!p.eof()) {
      if (err) *err = "trailing data after JSON value";
      throw std::runtime_error("trailing data after JSON value");
    }
    return true;
  } catch (const std::exception& e) {
    if (err) *err = e.what();
    return false;
  }
}

}  // namespace json
}  // namespace ferrocell
