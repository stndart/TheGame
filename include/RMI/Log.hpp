#pragma once

#include <cstdint>

namespace Rmi {

void LogC2sFramework(unsigned rmi_id, unsigned remote_count);
void LogC2sProxy(unsigned rmi_id, unsigned len);
void LogC2sFloor(unsigned rmi_id, unsigned len);
void LogS2c(unsigned rmi_id, unsigned len, unsigned host_id);

} // namespace Rmi
