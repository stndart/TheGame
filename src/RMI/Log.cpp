#include "RMI/Log.hpp"

#include <windows.h>

#include "thegame/log.hpp"

using thegame::logf;

void Rmi::LogC2sFramework(unsigned rmi_id, unsigned remote_count) {
  const unsigned id = rmi_id & 0xFFFFu;
  logf(thegame::LogMessage(
      thegame::LogSource::RMI,
      fmt::format("c2s frame id=0x%04X remotes={}", id, remote_count)));
}

void Rmi::LogC2sProxy(unsigned rmi_id, unsigned len) {
  const unsigned id = rmi_id & 0xFFFFu;
  logf(thegame::LogMessage(
      thegame::LogSource::RMI,
      fmt::format("c2s proxy id=0x%04X body[%u]", id, len)));
}

void Rmi::LogC2sFloor(unsigned rmi_id, unsigned len) {
  const unsigned id = rmi_id & 0xFFFFu;
  logf(thegame::LogMessage(
      thegame::LogSource::RMI,
      fmt::format("c2s floor id=0x%04X body[%u]", id, len)));
}

void Rmi::LogS2c(unsigned rmi_id, unsigned len, unsigned host_id) {
  const unsigned id = rmi_id & 0xFFFFu;
  logf(thegame::LogMessage(
      thegame::LogSource::RMI,
      fmt::format("s2c id=0x%04X host=%u body[%u]", id, host_id, len)));
}
