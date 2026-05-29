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

// TCP framing (CTcpLayerMessageExtractor::Extract / send framer — proudnet-sdk-crossmap §2)
constexpr std::uintptr_t kTcpFrameRecv = 0xD84BB0;
constexpr std::uintptr_t kTcpFrameRecvHook = kTcpFrameRecv;
constexpr std::uintptr_t kTcpFrameSend = 0xD84970;
// Hook at function entry only (not in-function E8 @ 0xD84910).
constexpr std::uintptr_t kTcpFrameSendHook = kTcpFrameSend;

// Message dispatch (sub_D653B0 cluster — see proudnet-message-dispatch-map.md)
constexpr std::uintptr_t kMessageRead = 0xD59300;
constexpr std::uintptr_t kMessageReadBit = 0xD58B30;
constexpr std::uintptr_t kMessageRestoreReadOffset = 0xD589C0;
constexpr std::uintptr_t kProcessProudNetLayer = 0xD653B0;
constexpr std::uintptr_t kProcessAltDispatch = 0xD366A0;
// sub_D65940 — CNetClientImpl::OnMessageReceived (SEH prologue 7 bytes; resume @ +7)
constexpr std::uintptr_t kDrainReceiveQueue = 0xD65940;
constexpr std::uintptr_t kDrainReceiveQueueResume = 0xD65947;
constexpr std::uintptr_t kIsFromRemoteClientPeer = 0xD5FD30;
constexpr std::uintptr_t kProcessCompressed = 0xD5DC10;
constexpr std::uintptr_t kProcessCompressedResume = 0xD5DC17;
constexpr std::uintptr_t kProcessEncrypted = 0xD5CA30;
constexpr std::uintptr_t kProcessEncryptedResume = 0xD5CA37;
constexpr std::uintptr_t kStackMessageCtor = 0xD12450;
constexpr std::uintptr_t kStackMessageDtor = 0xD124D0;
constexpr std::uintptr_t kEnqueueFormatError = 0xD83110;

constexpr std::uintptr_t kHandlerRmi = 0xD64F10;
constexpr std::uintptr_t kHandlerUserMessage = 0xD65170;
constexpr std::uintptr_t kHandlerHla = 0xD5FFD0;
constexpr std::uintptr_t kHandlerConnectServerTimedout = 0xD62930;
constexpr std::uintptr_t kHandlerGameNotify6 = 0xD62FD0;
constexpr std::uintptr_t kHandlerGameNotify8 = 0xD60020;
constexpr std::uintptr_t kHandlerGameNotify9 = 0xD60070;
constexpr std::uintptr_t kHandlerGameNotify10 = 0xD64760;
constexpr std::uintptr_t kHandlerNotifyProtocolVersionMismatch = 0xD5FF60;
constexpr std::uintptr_t kHandlerNotifyServerConnectSuccess = 0xD61490;
constexpr std::uintptr_t kHandlerNotifyAutoConnectionRecoverySuccess = 0xD645C0;
constexpr std::uintptr_t kHandlerRequestStartServerHolepunch = 0xD608D0;
constexpr std::uintptr_t kHandlerServerHolepunchAck = 0xD63510;
constexpr std::uintptr_t kHandlerGameNotify24 = 0xD63750;
constexpr std::uintptr_t kHandlerGameNotify25 = 0xD60A50;
constexpr std::uintptr_t kHandlerGameNotify26 = 0xD63A70;
constexpr std::uintptr_t kHandlerGameNotify29 = 0xD64D60;
constexpr std::uintptr_t kHandlerGameNotify31 = 0xD5FCD0;
constexpr std::uintptr_t kHandlerGameNotify32 = 0xD5FD00;
constexpr std::uintptr_t kHandlerGameNotify33 = 0xD61F20;
constexpr std::uintptr_t kHandlerGameNotify34 = 0xD60E10;
constexpr std::uintptr_t kHandlerGameNotify35 = 0xD625E0;
constexpr std::uintptr_t kHandlerGameNotify36 = 0xD61760;
constexpr std::uintptr_t kHandlerGameNotify40 = 0xD62110;
constexpr std::uintptr_t kHandlerGameNotify41 = 0xD60FB0;
constexpr std::uintptr_t kHandlerGameNotify49 = 0xD62330;
constexpr std::uintptr_t kHandlerGameNotify50 = 0xD61120;

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
