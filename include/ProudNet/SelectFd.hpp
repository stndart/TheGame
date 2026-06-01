#pragma once

#include "ProudNet/Layout.hpp"

// fd_set bundle helpers (sub_D551E0 / D55200 / D55250 / D552A0).
namespace Proud {

class CFastSocket;

namespace select_fd {

void init(Proud::CSelectContext *ctx);
void add_read(Proud::CSelectContext *ctx, Proud::CFastSocket *fast);
void add_except(Proud::CSelectContext *ctx, Proud::CFastSocket *fast);
int wait(Proud::CSelectContext *ctx, unsigned timeout_ms);

} // namespace select_fd
} // namespace Proud
