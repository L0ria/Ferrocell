// =============================================================================
//  Json — a minimal, dependency-free JSON value + parser + serializer (M5, #6).
//
//  Supports the RFC 8259 subset the Ferrocell API needs: object, array,
//  string, number, bool, null. It is PURE C++17 with NO Arduino / hardware
//  dependencies, so it compiles and is unit-tested on the host with
//  `g++ -std=c++17` (see tests/api_test.cpp). The same source is compiled
//  into the firmware (xtensa) where `UiServer` uses it to build/parse the
//  JSON API — keeping the exact same code path host- and device-side.
// =============================================================================
#ifndef FERROCELL_CORE_JSON_H
#define FERROCELL_CORE_JSON_H

#include <string>
#include <utility>
#include <vector>

namespace ferrocell {
namespace json {

// A JSON value (tagged union). `object` keeps insertion order so the
// serialized output is stable and readable.
class Value {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };

  Type type = Type::Null;
  bool boolean = false;
  double number = 0.0;
  std::string string;
  std::vector<Value> array;
  std::vector<std::pair<std::string, Value>> object;

  Value() = default;

  static Value makeNull();
  static Value makeBool(bool b);
  static Value makeNumber(double n);
  static Value makeNumber(int n);
  static Value makeString(const std::string& s);
  static Value makeArray();
  static Value makeObject();

  // --- type predicates ---
  bool isNull() const { return type == Type::Null; }
  bool isBool() const { return type == Type::Bool; }
  bool isNumber() const { return type == Type::Number; }
  bool isString() const { return type == Type::String; }
  bool isArray() const { return type == Type::Array; }
  bool isObject() const { return type == Type::Object; }

  // --- object access ---
  const Value* find(const std::string& key) const;
  bool has(const std::string& key) const { return find(key) != nullptr; }
  bool getBool(const std::string& key, bool def = false) const;
  double getNumber(const std::string& key, double def = 0.0) const;
  std::string getString(const std::string& key, const std::string& def = "") const;
  void set(const std::string& key, Value v);  // insert or replace (keeps order)

  // --- scalar coercion (return `def` if the type does not match) ---
  bool asBool(bool def = false) const;
  double asNumber(double def = 0.0) const;
  std::string asString(const std::string& def = "") const;
};

// Parse a JSON document into `out`. Returns true on success; on failure
// returns false and, if `err` is non-null, stores a short reason.
bool parse(const std::string& text, Value& out, std::string* err = nullptr);

// Serialize a Value to a compact JSON string (no insignificant whitespace).
std::string dump(const Value& v);

// Escape a string for use inside a JSON string literal (quotes NOT included).
std::string escape(const std::string& s);

}  // namespace json
}  // namespace ferrocell

#endif  // FERROCELL_CORE_JSON_H
