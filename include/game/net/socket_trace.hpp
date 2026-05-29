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

// True when socket was connect-tracked on a PN leg or peer port matches.
bool is_tracked_pn_socket(SOCKET s);

// Peer port, or tracked leg port when getpeername is not ready yet.
u_short pn_log_port(SOCKET s);

// Peer TCP port, or 0 if unknown / not connected.
u_short peer_port(SOCKET s);

// netlogs.txt line prefix: peer port when known, else socket handle.
int net_log_key(SOCKET s);

bool is_entry_socket(SOCKET s);
bool is_game_server_socket(SOCKET s); // legacy: tracked or peer :7000

bool peer_is_entry_server(SOCKET s);

SOCKET read_fast_socket_slot(void *fast_socket_ctx);

// When PN FSM polls Proud::CFastSocket+300, fix only an empty/wrong ENTRY slot - never
// stomp :27380.
void sync_fast_socket_handle(void *fast_socket_ctx);

// Last Proud::CFastSocket ctx seen in sub_D55300 (pn_select); used by recv-append
// logging.
void note_fast_socket_ctx(void *fast_socket_ctx);
void *last_fast_socket_ctx();
SOCKET socket_from_last_fast_ctx();

// Peer port from getpeername, else embedded Proud::AddrPort on the fast socket.
u_short fast_socket_log_port(void *fast_socket_ctx);

} // namespace SocketTrace
