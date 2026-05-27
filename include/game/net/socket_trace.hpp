#pragma once

#include <winsock2.h>

namespace SocketTrace {

// FA-EMU / ProudNet legs (offline remap uses same ports).
constexpr u_short kEntryPort = 7000;
constexpr u_short kGameLegPort = 27380;
constexpr u_short kProbePort = 20009;
constexpr u_short kGameServerPort = kEntryPort; // legacy name

void track_connect(SOCKET s, u_short port);

bool is_pn_track_port(u_short port);

// Peer TCP port, or 0 if unknown / not connected.
u_short peer_port(SOCKET s);

// netlogs.txt line prefix: peer port when known, else socket handle.
int net_log_key(SOCKET s);

bool is_entry_socket(SOCKET s);
bool is_game_server_socket(SOCKET s); // legacy: tracked or peer :7000

bool peer_is_entry_server(SOCKET s);

SOCKET read_fast_socket_slot(void *fast_socket_ctx);

// When PN FSM polls CFastSocket+300, fix only an empty/wrong ENTRY slot — never stomp :27380.
void sync_fast_socket_handle(void *fast_socket_ctx);

} // namespace SocketTrace
