#pragma once

#include <cstdint>

// Proud::MessageType ordinals (uint8 on wire via Message_Read).
// v1.8 PDB names from docs/proudnet-sdk-crossmap.md §6; GAME.exe remaps noted
// inline.
namespace Proud {
namespace MessageType {

enum class Type : std::uint8_t {
  // §6 - core
  Rmi = 1,
  UserMessage = 2,
  Hla = 3,
  ConnectServerTimedout = 4,
  NotifyStartupEnvironment = 5, // GAME sub_D653B0: default (unhandled)
  // 6 - GAME-only handler @ 0xD62FD0 (+ peer guard)
  GameNotify6 = 6,
  // 7 - GAME default (noop)
  GameReserved7 = 7,
  // 8–10 - GAME handlers (+ peer guard); §6 UDP family partial overlap
  GameNotify8 = 8,
  GameNotify9 = 9,
  GameNotify10 = 10,
  NotifyProtocolVersionMismatch = 11,
  NotifyServerDeniedConnection = 12, // GAME: default
  NotifyServerConnectSuccess = 13,
  // 14 - GAME default (noop)
  GameReserved14 = 14,
  NotifyAutoConnectionRecoverySuccess = 15,
  NotifyAutoConnectionRecoveryFailed = 16, // GAME: default
  RequestStartServerHolepunch = 17,
  // 18 - GAME default
  GameReserved18 = 18,
  ServerHolepunchAck = 19,
  // 20–23 - GAME default block
  GameReserved20 = 20,
  NotifyClientServerUdpMatched = 21,
  GameReserved22 = 22,
  PeerUdp_ServerHolepunchAck = 23,
  GameNotify24 = 24,
  GameNotify25 = 25,
  GameNotify26 = 26,
  // 27–28 - GAME default
  GameReserved27 = 27,
  GameReserved28 = 28,
  GameNotify29 = 29,
  GameNoop30 = 30, // explicit noop in GAME switch
  GameNotify31 = 31,
  GameNotify32 = 32,
  GameNotify33 = 33,
  GameNotify34 = 34,
  GameNotify35 = 35,
  GameNotify36 = 36,
  // GAME compress (v1.8 MessageType_Compressed = 47)
  GameCompressed37 = 37,
  GameCompressed38 = 38,
  // GAME encrypt (v1.8 encrypted family 43–46 collapsed)
  GameEncrypted39 = 39,
  GameNotify40 = 40,
  GameNotify41 = 41,
  // 42–46 - GAME default (v1.8 encrypted variants)
  GameEncrypted42 = 42,
  GameEncrypted43 = 43,
  GameEncrypted44 = 44,
  GameEncrypted45 = 45,
  GameEncrypted46 = 46,
  MessageType_Compressed =
      47, // v1.8 PDB; GAME client switch: default (not 37–38)
  GameNoop48 = 48,
  GameNotify49 = 49,
  GameNotify50 = 50,
  // §6 - not in GAME sub_D653B0 switch arms
  S2CMulticast54 = 54,
  S2CMulticast55 = 55,
  S2CMulticast56 = 56,
};

constexpr bool is_game_noop(Type t) {
  switch (t) {
  case Type::GameReserved7:
  case Type::GameReserved14:
  case Type::GameReserved18:
  case Type::GameReserved20:
  case Type::GameReserved22:
  case Type::GameReserved27:
  case Type::GameReserved28:
  case Type::GameNoop30:
  case Type::GameNoop48:
    return true;
  default:
    return false;
  }
}

} // namespace MessageType
} // namespace Proud
