#include "target_hooks.h"

#include "game/net/pn_select.hpp"

extern "C" void __declspec(naked) hook_pn_select() {
  __asm {
    jmp PNSelectContext::poll
  }
}

HookStub g_target_pn_select = {0xD55300,
                               (uint32_t)(uintptr_t)hook_pn_select,
                               "hook_pn_select",
                               {0},
                               false,
                               0};
