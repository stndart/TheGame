#pragma once

// CNetClientImpl::OnMessageReceived (GAME sub_D65940).
// __thiscall: ECX = net_client, ret 4. Worker @ [client+0x5C] (not a stack arg).
void drain_receive_queue(void *net_client);
