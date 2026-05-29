#pragma once

namespace Proud {

// CNetClientImpl::OnMessageReceived (GAME sub_D65940).
void DrainReceiveQueue(void *net_client);
void DrainReceiveQueueCallOriginal(void *net_client);

} // namespace Proud
