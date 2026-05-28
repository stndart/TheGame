#pragma once

#include "game/net/pn_layout.hpp"

class PNFastSocket;

// fd_set bundle helpers (sub_D551E0 / D55200 / D55250 / D552A0).
namespace pn {
namespace select_fd {

void init(PNSelectContext *ctx);
void add_read(PNSelectContext *ctx, PNFastSocket *fast);
void add_except(PNSelectContext *ctx, PNFastSocket *fast);
int wait(PNSelectContext *ctx, unsigned timeout_ms);

} // namespace select_fd
} // namespace pn
