#include "ProudNet/DrainReceiveQueue.hpp"

#include "thegame/config.hpp"
#include "thegame/log.hpp"

using thegame::logpnf;

namespace {

constexpr int kLogLimit = 8;
constexpr std::uintptr_t kNetClientWorkerField = 0x5C;

void *worker_from_client(void *net_client) {
  if (!net_client) {
    return nullptr;
  }
  return *reinterpret_cast<void **>(reinterpret_cast<char *>(net_client) +
                                    kNetClientWorkerField);
}

} // namespace

void Proud::DrainReceiveQueue(void *net_client) {
  static int logs = 0;
  if (logs < kLogLimit && !thegame::cfg.no_proud_logs) {
    logpnf(0, "drain_recv: client=%p worker=%p", net_client,
           worker_from_client(net_client));
    ++logs;
  }
  // NOTE: in-process S2C inject was removed (v1); friends server is authoritative.
  // See docs/rmi/fake-server-hooks.md.
}

void Proud::DrainReceiveQueueCallOriginal(void *net_client) {
  (void)net_client;
}
