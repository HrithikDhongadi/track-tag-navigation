#pragma once

#include <cctype>
#include <optional>
#include <string>

namespace amr {

inline std::string escapeJson(const std::string &value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const unsigned char character : value) {
    switch (character) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if (character < 0x20) {
          constexpr char hex[] = "0123456789abcdef";
          escaped += "\\u00";
          escaped += hex[(character >> 4) & 0x0f];
          escaped += hex[character & 0x0f];
        } else {
          escaped += static_cast<char>(character);
        }
    }
  }
  return escaped;
}

// Small, dependency-free reader for the flat string fields used in transport
// status messages. Producers own the schema; consumers never parse display text.
inline std::optional<std::string> jsonStringField(const std::string &json,
                                                   const std::string &key) {
  const std::string needle = "\"" + key + "\"";
  const auto keyStart = json.find(needle);
  if (keyStart == std::string::npos) return std::nullopt;
  auto cursor = keyStart + needle.size();
  while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) ++cursor;
  if (cursor == json.size() || json[cursor++] != ':') return std::nullopt;
  while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) ++cursor;
  if (cursor == json.size() || json[cursor++] != '"') return std::nullopt;
  std::string value;
  while (cursor < json.size()) {
    const char character = json[cursor++];
    if (character == '"') return value;
    if (character != '\\' || cursor == json.size()) { value += character; continue; }
    switch (json[cursor++]) {
      case '"': value += '"'; break;
      case '\\': value += '\\'; break;
      case 'n': value += '\n'; break;
      case 'r': value += '\r'; break;
      case 't': value += '\t'; break;
      default: return std::nullopt;
    }
  }
  return std::nullopt;
}

}  // namespace amr
