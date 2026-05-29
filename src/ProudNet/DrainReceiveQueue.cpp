#include "ProudNet/DrainReceiveQueue.hpp"

#include "console.h"

namespace {

constexpr int kLogLimit = 8;
constexpr std::uintptr_t kNetClientWorkerField = 0x5C;

void *worker_from_client(void *net_client) {
  if (!net_client) {
    return nullptr;
  }
  return *reinterpret_cast<void **>(
      reinterpret_cast<char *>(net_client) + kNetClientWorkerField);
}

} // namespace

void Proud::DrainReceiveQueue(void *net_client) {
  static int logs = 0;
  if (logs < kLogLimit) {
    logf("pn_drain_recv: client=%p worker=%p", net_client,
         worker_from_client(net_client));
    ++logs;
  }
  // NOTE: S2C RES injection used to be pumped here, but this hook runs on the
  // ProudNet worker thread. The RES leaves drive RequestState/UI/scripting-VM
  // work that must run on the main (frame) thread; pumping here raced the frame
  // loop and intermittently faulted the UI VM. Injection now pumps from the
  // IState::onPreProcess hooks (main thread). See ProudNet/RmiInject.{hpp,cpp}.
}

void Proud::DrainReceiveQueueCallOriginal(void *net_client) {
  (void)net_client;
}
