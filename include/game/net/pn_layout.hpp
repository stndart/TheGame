#pragma once

#include <cstdint>

// ProudNet client object layout (GAME.exe IDA cluster, May 2026).
// Method bodies: pn_fast_socket.hpp, pn_connection.hpp, pn_growable.hpp.

class PNFastSocket;
class PNSelectContext;
class PNUpnpClient;

namespace pn {
namespace rva {

constexpr std::uintptr_t kGrowableEnsure = 0x9FC720;
constexpr std::uintptr_t kGrowableCalcCap = 0x9FCB70;
constexpr std::uintptr_t kFmtErrorX2 = 0xCEFAE0;
constexpr std::uintptr_t kSelectPoll = 0xD55300;
constexpr std::uintptr_t kRecvComplete = 0xD55510;
constexpr std::uintptr_t kSendComplete = 0xD55660;
constexpr std::uintptr_t kRecvStagingPtr = 0xD558A0;
constexpr std::uintptr_t kSocketReportError = 0xD56170;
constexpr std::uintptr_t kFastRecv = 0xD56470;
constexpr std::uintptr_t kFastSend = 0xD567F0;
constexpr std::uintptr_t kSendArm = 0xD6C880;
constexpr std::uintptr_t kFsmState2 = 0xD6C8E0;
constexpr std::uintptr_t kFsmState3 = 0xD6C9C0;
constexpr std::uintptr_t kFsmState1 = 0xD6EF90;
constexpr std::uintptr_t kFsmDispatch = 0xD6F7B0;
constexpr std::uintptr_t kRecvAppend = 0xD71CE0;
constexpr std::uintptr_t kRecvEnsure = 0xD71610;
constexpr std::uintptr_t kUpnpAddPort = 0xD6E180;
constexpr std::uintptr_t kUpnpDeletePort = 0xD6E5B0;
constexpr std::uintptr_t kNetClientCtor = 0xD0A340;
constexpr std::uintptr_t kNetClientFactory = 0xD0C0A0;

} // namespace rva

namespace growable {

constexpr std::uintptr_t kHeader = 0;
constexpr std::uintptr_t kAllocator = 4;
constexpr std::uintptr_t kData = 8;
constexpr std::uintptr_t kSize = 12;
constexpr std::uintptr_t kCapacity = 16;
constexpr std::uintptr_t kMinCapacity = 20;
constexpr std::uintptr_t kGrowthMode = 24;

} // namespace growable

namespace conn {

constexpr std::uintptr_t kSendArm = 4;
constexpr std::uintptr_t kFastSocket = 8;
constexpr std::uintptr_t kRecvBuffer = 32;
constexpr std::uintptr_t kFsmState = 64;
constexpr std::uintptr_t kRecvPending = 20;
constexpr std::uintptr_t kSendPendingConn = 16;
constexpr std::uintptr_t kSendString = 12;
constexpr std::uintptr_t kSendBytesDone = 24;
constexpr std::uintptr_t kRecvBytesTotal = 28;
constexpr std::uintptr_t kUpnpSubmode = 68;
constexpr std::uintptr_t kGatewayCtx = 72;

} // namespace conn

namespace fast_socket {

constexpr std::uintptr_t kCallback = 16;
constexpr std::uintptr_t kReportEnabled = 21;
constexpr std::uintptr_t kOverlapped = 116;
constexpr std::uintptr_t kOverlapInFlight = 116; // dword == 259 while I/O pending
constexpr std::uintptr_t kRecvGrowable = 0x8C;   // embedded PNGrowableBuffer @ +140
constexpr std::uintptr_t kRecvFlags = 0xF8;      // WSARecv flags dword @ +248
constexpr std::uintptr_t kRecvPending = 136;
constexpr std::uintptr_t kRecvIssueWarning = 136; // duplicate recv arm warning
constexpr std::uintptr_t kRecvStagingPtr = 0x94;
constexpr std::uintptr_t kRecvStagingActive = 0x98;
constexpr std::uintptr_t kSendWarning = 0xBC;
constexpr std::uintptr_t kSendOverlap = 168;
constexpr std::uintptr_t kStaging = 0xC0;
constexpr std::uintptr_t kSendPending = 0x12A;
constexpr std::uintptr_t kSendOpCount = 0x104;
constexpr std::uintptr_t kSocket = 0x12C;
constexpr std::uintptr_t kAddrPort = 0xE4; // Proud::AddrPort; sub_CF0460 updates from out[]

} // namespace fast_socket

namespace select_ctx {

constexpr std::uintptr_t kReadFdSet = 0x104;
constexpr std::uintptr_t kExceptFdSet = 0x208;

} // namespace select_ctx

} // namespace pn

struct PNGrowableBuffer {
  std::uint32_t header_;
  void *allocator_;
  char *data_;
  int size_;
  int capacity_;
  int min_capacity_;
  int growth_mode_;
};

struct PNRecvBuffer : PNGrowableBuffer {
  void ensure_capacity(int new_size);
  void append(const char *src, int len);
};

class PNSelectContext {
public:
  char poll(void *fast_socket_ctx, std::uint32_t *out);
};

class PNUpnpClient {
public:
  void AddPortMapping();
  void DeletePortMapping();
};
