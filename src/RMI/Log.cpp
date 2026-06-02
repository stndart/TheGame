#include "RMI/Log.hpp"

#include <windows.h>

#include "thegame/log.hpp"

using thegame::logf;

void Rmi::LogC2sFramework(unsigned rmi_id, unsigned remote_count) {
  const unsigned id = rmi_id & 0xFFFFu;
  char buf[96];
  wsprintfA(buf, "[rmi] c2s framework id=0x%04X (%u) remotes=%u", id, id,
            remote_count);
  logf(buf);
}

void Rmi::LogC2sProxy(unsigned rmi_id, unsigned len) {
  const unsigned id = rmi_id & 0xFFFFu;
  char buf[96];
  wsprintfA(buf, "[rmi] c2s proxy id=0x%04X (%u) len=%u", id, id, len);
  logf(buf);
}

void Rmi::LogC2sFloor(unsigned rmi_id, unsigned len) {
  const unsigned id = rmi_id & 0xFFFFu;
  char buf[96];
  wsprintfA(buf, "[rmi] c2s floor id=0x%04X (%u) len=%u", id, id, len);
  logf(buf);
}

void Rmi::LogS2c(unsigned rmi_id, unsigned len, unsigned host_id) {
  const unsigned id = rmi_id & 0xFFFFu;
  char buf[96];
  wsprintfA(buf, "[rmi] s2c id=0x%04X (%u) len=%u host=%u", id, id, len,
            host_id);
  logf(buf);
}
