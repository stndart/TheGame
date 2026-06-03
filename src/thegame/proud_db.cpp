#include "thegame/proud_db.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <windows.h>

#include <fmt/format.h>

#include "thegame/log.hpp"
#include "thegame/paths.hpp"

namespace {

using json = nlohmann::json;

struct ProudDb {
  std::unordered_map<uint8_t, std::string> opcodes;
  std::unordered_map<uint16_t, std::string> rmi_s2c;
  std::unordered_map<uint16_t, std::string> rmi_c2s;
  bool loaded{false};
};

ProudDb g_db;
std::once_flag g_load_once;

std::string trim_ascii(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.erase(s.begin());
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
  return s;
}

bool parse_uint64(const json &value, uint64_t &out) {
  if (value.is_number_unsigned()) {
    out = value.get<uint64_t>();
    return true;
  }
  if (value.is_number_integer()) {
    const auto n = value.get<int64_t>();
    if (n < 0)
      return false;
    out = static_cast<uint64_t>(n);
    return true;
  }
  if (!value.is_string())
    return false;

  std::string s = trim_ascii(value.get<std::string>());
  if (s.empty())
    return false;

  char *end = nullptr;
  const int base =
      (s.size() >= 2 && s[0] == '0' &&
       (s[1] == 'x' || s[1] == 'X'))
          ? 16
          : 10;
  const unsigned long long n = std::strtoull(s.c_str(), &end, base);
  if (end == s.c_str() || (end && *end != '\0'))
    return false;
  out = n;
  return true;
}

bool parse_uint8_key(const std::string &key, uint8_t &out) {
  json wrapped = key;
  uint64_t n = 0;
  if (!parse_uint64(wrapped, n) || n > 0xFF)
    return false;
  out = static_cast<uint8_t>(n);
  return true;
}

void load_rmi_table(const json &obj, std::unordered_map<uint16_t, std::string> &out) {
  if (!obj.is_object())
    return;
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    uint64_t id = 0;
    if (!parse_uint64(it.value(), id) || id > 0xFFFF)
      continue;
    out[static_cast<uint16_t>(id)] = it.key();
  }
}

void load_from_json(const json &root) {
  g_db.opcodes.clear();
  g_db.rmi_s2c.clear();
  g_db.rmi_c2s.clear();

  if (root.contains("Opcodes") && root["Opcodes"].is_object()) {
    for (auto it = root["Opcodes"].begin(); it != root["Opcodes"].end(); ++it) {
      uint8_t opcode = 0;
      if (!parse_uint8_key(it.key(), opcode))
        continue;
      if (it.value().is_string())
        g_db.opcodes[opcode] = it.value().get<std::string>();
    }
  }

  if (root.contains("RMI") && root["RMI"].is_object()) {
    const json &rmi = root["RMI"];
    if (rmi.contains("S2C"))
      load_rmi_table(rmi["S2C"], g_db.rmi_s2c);
    if (rmi.contains("C2S"))
      load_rmi_table(rmi["C2S"], g_db.rmi_c2s);
  }

  g_db.loaded = true;
}

std::string resolve_rmi_name(uint16_t rmi_id) {
  const auto s2c = g_db.rmi_s2c.find(rmi_id);
  const auto c2s = g_db.rmi_c2s.find(rmi_id);
  if (s2c != g_db.rmi_s2c.end() && c2s != g_db.rmi_c2s.end())
    return fmt::format("S2C:{} / C2S:{}", s2c->second, c2s->second);
  if (s2c != g_db.rmi_s2c.end())
    return fmt::format("S2C:{}", s2c->second);
  if (c2s != g_db.rmi_c2s.end())
    return fmt::format("C2S:{}", c2s->second);
  return {};
}

void load_proud_db_once() {
  const std::string path = thegame::proud_db_path();
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    thegame::logf(thegame::LogMessage(
        thegame::LogSource::Log, thegame::LogImportance::Warning,
        "proud_db: could not open {}", path));
    return;
  }

  json root;
  try {
    in >> root;
    load_from_json(root);
  } catch (const std::exception &ex) {
    thegame::logf(thegame::LogMessage(
        thegame::LogSource::Log, thegame::LogImportance::Warning,
        "proud_db: parse error in {}: {}", path, ex.what()));
    return;
  }

  thegame::logf(thegame::LogMessage(
      thegame::LogSource::Log, thegame::LogImportance::Milestone,
      "proud_db: loaded {} opcodes, {} S2C RMIs, {} C2S RMIs from {}",
      g_db.opcodes.size(), g_db.rmi_s2c.size(), g_db.rmi_c2s.size(), path));
}

} // namespace

namespace thegame {

std::string proud_db_path() {
  char env[MAX_PATH];
  const DWORD n = GetEnvironmentVariableA("THEGAME_PROUD_DB", env, sizeof(env));
  if (n > 0 && n < sizeof(env))
    return std::string(env, n);

  const std::string dir = game_directory();
  if (dir.empty())
    return kProudDbJson;
  return dir + "\\" + kProudDbJson;
}

void init_proud_db() {
  std::call_once(g_load_once, load_proud_db_once);
}

} // namespace thegame

void generate_frame_comment(uint8_t opcode, uint16_t rmi_id, bool has_rmi,
                            bool &known, std::string &comment) {
  std::call_once(g_load_once, load_proud_db_once);

  known = false;
  comment.clear();

  const auto op = g_db.opcodes.find(opcode);
  if (op != g_db.opcodes.end()) {
    known = true;
    comment = op->second;
  }

  if (!has_rmi) {
    if (!known)
      comment = fmt::format("opcode 0x{:02x}", opcode);
    return;
  }

  const std::string rmi_name = resolve_rmi_name(rmi_id);
  if (!rmi_name.empty()) {
    known = true;
    if (comment.empty())
      comment = rmi_name;
    else
      comment += " " + rmi_name;
    return;
  }

  if (comment.empty())
    comment = fmt::format("rmi 0x{:04x}", rmi_id);
  else
    comment += fmt::format(" rmi 0x{:04x}", rmi_id);
}
