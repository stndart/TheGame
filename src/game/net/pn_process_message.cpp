#include "game/net/pn_process_message.hpp"

#include "game/net/pn_layout.hpp"
#include "game/net/pn_message_type.hpp"
#include "hook_manager.h"
#include "target_hooks.h"

#include <cstddef>
#include <windows.h>

namespace {

uintptr_t game_va(const std::uint32_t va) {
  const uintptr_t delta =
      reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)) - 0x400000u;
  return static_cast<uintptr_t>(va) + delta;
}

using MessageReadFn = char(__thiscall *)(int *msg_stream, std::uint32_t *out_type);
using RestoreReadOffsetFn = int(__thiscall *)(int *msg_stream, int offset_div8);
using IsFromRemoteClientPeerFn = bool(__thiscall *)(void *worker, void *msg);
using StackMessageCtorFn = void *(__thiscall *)(void *stack_mem);
using StackMessageDtorFn = void(__thiscall *)(void *stack_mem);
using ProcessCompressedFn = char(__thiscall *)(void *core, std::uint32_t type,
                                             void *received_msg, void *stack_mem);
using ProcessEncryptedFn = char(__thiscall *)(void *core, void *received_msg,
                                            void *stack_mem);

using WorkerMsgFn = void(__thiscall *)(void *worker, void *msg);
using WorkerRmiFn = int(__thiscall *)(void *worker, void *msg,
                                      unsigned char *out_processed);
using WorkerUserMessageFn = int(__thiscall *)(void *worker, void *msg,
                                              void *out_byte);
using WorkerWstrMsgFn = void(__thiscall *)(void *worker, void *wstr_body,
                                           void *msg);

constexpr std::size_t kStackReceivedMessageBytes = 0x30;

char call_process_compressed(void *core, std::uint32_t type, void *received_msg,
                             void *stack_mem) {
  HookManager::restore_hook(g_target_pn_process_compressed);
  const auto orig = reinterpret_cast<ProcessCompressedFn>(
      game_va(static_cast<std::uint32_t>(pn::rva::kProcessCompressed)));
  const char result = orig(core, type, received_msg, stack_mem);
  HookManager::make_hook(g_target_pn_process_compressed);
  return result;
}

char call_process_encrypted(void *core, void *received_msg, void *stack_mem) {
  HookManager::restore_hook(g_target_pn_process_encrypted);
  const auto orig = reinterpret_cast<ProcessEncryptedFn>(
      game_va(static_cast<std::uint32_t>(pn::rva::kProcessEncrypted)));
  const char result = orig(core, received_msg, stack_mem);
  HookManager::make_hook(g_target_pn_process_encrypted);
  return result;
}

void *worker_net_core(void *worker) {
  return *reinterpret_cast<void **>(reinterpret_cast<char *>(worker) + 0x5C);
}

void restore_read_offset(void *received_msg, int offset_div8) {
  const auto fn = reinterpret_cast<RestoreReadOffsetFn>(
      game_va(static_cast<std::uint32_t>(pn::rva::kMessageRestoreReadOffset)));
  fn(reinterpret_cast<int *>(received_msg), offset_div8);
}

void copy_received_metadata(void *stack_mem, const void *received_msg) {
  auto *const dst = reinterpret_cast<unsigned char *>(stack_mem);
  const auto *const src = reinterpret_cast<const unsigned char *>(received_msg);
  *reinterpret_cast<std::uint32_t *>(dst + 0x1C) =
      *reinterpret_cast<const std::uint32_t *>(src + 0x1C);
  *reinterpret_cast<std::uint32_t *>(dst + 0x24) =
      *reinterpret_cast<const std::uint32_t *>(src + 0x24);
  *reinterpret_cast<std::uint16_t *>(dst + 0x28) =
      *reinterpret_cast<const std::uint16_t *>(src + 0x28);
  dst[0x2C] = src[0x2C];
}

bool is_from_remote_client_peer(void *worker, void *msg) {
  const auto fn = reinterpret_cast<IsFromRemoteClientPeerFn>(game_va(
      static_cast<std::uint32_t>(pn::rva::kIsFromRemoteClientPeer)));
  return fn(worker, msg);
}

void call_worker_msg_handler(void *worker, std::uintptr_t rva, void *msg) {
  const auto fn = reinterpret_cast<WorkerMsgFn>(
      game_va(static_cast<std::uint32_t>(rva)));
  fn(worker, msg);
}

bool dispatch_with_peer_guard(void *worker, void *msg, std::uintptr_t rva) {
  if (!is_from_remote_client_peer(worker, msg)) {
    call_worker_msg_handler(worker, rva, msg);
  }
  return true;
}

bool dispatch_worker_wstr_msg(void *worker, void *wstr, void *msg,
                              std::uintptr_t rva) {
  const auto fn = reinterpret_cast<WorkerWstrMsgFn>(
      game_va(static_cast<std::uint32_t>(rva)));
  fn(worker, wstr, msg);
  return true;
}

bool dispatch_compressed_or_encrypted(void *worker, void *wstr, void *msg,
                                    pn::MessageType::Type type) {
  alignas(16) unsigned char stack_mem[kStackReceivedMessageBytes]{};

  const auto ctor = reinterpret_cast<StackMessageCtorFn>(game_va(
      static_cast<std::uint32_t>(pn::rva::kStackMessageCtor)));
  const auto dtor = reinterpret_cast<StackMessageDtorFn>(game_va(
      static_cast<std::uint32_t>(pn::rva::kStackMessageDtor)));
  ctor(stack_mem);

  void *const core = worker_net_core(worker);
  bool unwrapped = false;
  if (type == pn::MessageType::Type::GameEncrypted39) {
    unwrapped = call_process_encrypted(core, msg, stack_mem) != 0;
  } else {
    unwrapped = call_process_compressed(core, static_cast<std::uint32_t>(type),
                                        msg, stack_mem) != 0;
  }

  if (!unwrapped) {
    dtor(stack_mem);
    return false;
  }

  copy_received_metadata(stack_mem, msg);
  const bool processed =
      process_message_proudnet_layer(worker, wstr, stack_mem);
  dtor(stack_mem);
  return processed;
}

} // namespace

bool process_message_proudnet_layer(void *worker, void *wstr_body,
                                    void *received_msg) {
  auto *const msg_stream = reinterpret_cast<int *>(received_msg);
  const int saved_offset_div8 = *msg_stream >> 3;

  std::uint32_t type_raw = 0;
  const auto message_read = reinterpret_cast<MessageReadFn>(game_va(
      static_cast<std::uint32_t>(pn::rva::kMessageRead)));
  if (!message_read(msg_stream, &type_raw)) {
    restore_read_offset(received_msg, saved_offset_div8);
    return false;
  }

  const auto type = static_cast<pn::MessageType::Type>(
      static_cast<std::uint8_t>(type_raw));
  bool message_processed = false;

  switch (type) {
  case pn::MessageType::Type::Rmi: {
    unsigned char out_flag = 0;
    const auto fn = reinterpret_cast<WorkerRmiFn>(
        game_va(static_cast<std::uint32_t>(pn::rva::kHandlerRmi)));
    fn(worker, received_msg, &out_flag);
    message_processed = out_flag != 0;
    break;
  }
  case pn::MessageType::Type::UserMessage: {
    unsigned char out_flag = 0;
    const auto fn = reinterpret_cast<WorkerUserMessageFn>(game_va(
        static_cast<std::uint32_t>(pn::rva::kHandlerUserMessage)));
    fn(worker, received_msg, &out_flag);
    (void)out_flag;
    // GAME case 2: handler runs but bl stays 0 → return 0 (no LABEL_53).
    message_processed = false;
    break;
  }
  case pn::MessageType::Type::Hla:
    message_processed =
        dispatch_with_peer_guard(worker, received_msg, pn::rva::kHandlerHla);
    break;
  case pn::MessageType::Type::ConnectServerTimedout:
    // TODO: OutputDebugStringW before handler (sub_D653B0 case 4).
    message_processed = dispatch_with_peer_guard(
        worker, received_msg, pn::rva::kHandlerConnectServerTimedout);
    break;
  case pn::MessageType::Type::GameNotify6:
    message_processed = dispatch_with_peer_guard(worker, received_msg,
                                                 pn::rva::kHandlerGameNotify6);
    break;
  case pn::MessageType::Type::GameNotify8:
    message_processed = dispatch_with_peer_guard(worker, received_msg,
                                                 pn::rva::kHandlerGameNotify8);
    break;
  case pn::MessageType::Type::GameNotify9:
    message_processed = dispatch_with_peer_guard(worker, received_msg,
                                                 pn::rva::kHandlerGameNotify9);
    break;
  case pn::MessageType::Type::GameNotify10:
    message_processed = dispatch_with_peer_guard(worker, received_msg,
                                                 pn::rva::kHandlerGameNotify10);
    break;
  case pn::MessageType::Type::NotifyProtocolVersionMismatch:
    message_processed = dispatch_with_peer_guard(
        worker, received_msg, pn::rva::kHandlerNotifyProtocolVersionMismatch);
    break;
  case pn::MessageType::Type::NotifyServerConnectSuccess:
    message_processed = dispatch_with_peer_guard(
        worker, received_msg, pn::rva::kHandlerNotifyServerConnectSuccess);
    break;
  case pn::MessageType::Type::NotifyAutoConnectionRecoverySuccess:
    message_processed = dispatch_with_peer_guard(
        worker, received_msg,
        pn::rva::kHandlerNotifyAutoConnectionRecoverySuccess);
    break;
  case pn::MessageType::Type::RequestStartServerHolepunch:
    message_processed = dispatch_with_peer_guard(
        worker, received_msg, pn::rva::kHandlerRequestStartServerHolepunch);
    break;
  case pn::MessageType::Type::ServerHolepunchAck:
    message_processed =
        dispatch_worker_wstr_msg(worker, wstr_body, received_msg,
                                 pn::rva::kHandlerServerHolepunchAck);
    break;
  case pn::MessageType::Type::GameNotify24:
    message_processed =
        dispatch_with_peer_guard(worker, received_msg, pn::rva::kHandlerGameNotify24);
    break;
  case pn::MessageType::Type::GameNotify25:
    if (!is_from_remote_client_peer(worker, received_msg)) {
      dispatch_worker_wstr_msg(worker, wstr_body, received_msg,
                               pn::rva::kHandlerGameNotify25);
    }
    message_processed = true;
    break;
  case pn::MessageType::Type::GameNotify26:
    if (!is_from_remote_client_peer(worker, received_msg)) {
      dispatch_worker_wstr_msg(worker, wstr_body, received_msg,
                               pn::rva::kHandlerGameNotify26);
    }
    message_processed = true;
    break;
  case pn::MessageType::Type::GameNotify29:
    message_processed =
        dispatch_with_peer_guard(worker, received_msg, pn::rva::kHandlerGameNotify29);
    break;
  case pn::MessageType::Type::GameNotify31:
    call_worker_msg_handler(worker, pn::rva::kHandlerGameNotify31, received_msg);
    message_processed = true;
    break;
  case pn::MessageType::Type::GameNotify32:
    call_worker_msg_handler(worker, pn::rva::kHandlerGameNotify32, received_msg);
    message_processed = true;
    break;
  case pn::MessageType::Type::GameNotify33:
    call_worker_msg_handler(worker, pn::rva::kHandlerGameNotify33, received_msg);
    message_processed = true;
    break;
  case pn::MessageType::Type::GameNotify34:
    call_worker_msg_handler(worker, pn::rva::kHandlerGameNotify34, received_msg);
    message_processed = true;
    break;
  case pn::MessageType::Type::GameNotify35:
    if (!is_from_remote_client_peer(worker, received_msg)) {
      dispatch_worker_wstr_msg(worker, wstr_body, received_msg,
                               pn::rva::kHandlerGameNotify35);
    }
    message_processed = true;
    break;
  case pn::MessageType::Type::GameNotify36:
    dispatch_worker_wstr_msg(worker, wstr_body, received_msg,
                             pn::rva::kHandlerGameNotify36);
    message_processed = true;
    break;
  case pn::MessageType::Type::GameCompressed37:
  case pn::MessageType::Type::GameCompressed38:
  case pn::MessageType::Type::GameEncrypted39:
    return dispatch_compressed_or_encrypted(worker, wstr_body, received_msg,
                                            type);
  case pn::MessageType::Type::GameNotify40:
    dispatch_worker_wstr_msg(worker, wstr_body, received_msg,
                             pn::rva::kHandlerGameNotify40);
    message_processed = true;
    break;
  case pn::MessageType::Type::GameNotify41:
    call_worker_msg_handler(worker, pn::rva::kHandlerGameNotify41, received_msg);
    message_processed = true;
    break;
  case pn::MessageType::Type::GameNotify49:
    call_worker_msg_handler(worker, pn::rva::kHandlerGameNotify49, received_msg);
    message_processed = true;
    break;
  case pn::MessageType::Type::GameNotify50:
    call_worker_msg_handler(worker, pn::rva::kHandlerGameNotify50, received_msg);
    message_processed = true;
    break;
  default:
    if (pn::MessageType::is_game_noop(type)) {
      message_processed = true;
    }
    break;
  }

  if (!message_processed) {
    restore_read_offset(received_msg, saved_offset_div8);
    return false;
  }

  // TODO: post-switch unconsumed-byte check (sub_D83110) when type not 37/38.
  return true;
}
