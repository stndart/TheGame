#include "target_hooks.h"

#include "ProudNet/SelectContext.hpp"

extern "C" void __declspec(naked) hook_pn_select() {
  __asm {
    jmp Proud::CSelectContext::poll
  }
}

HookStub g_target_pn_select = {0xD55300,
                               (uint32_t)(uintptr_t)hook_pn_select,
                               "hook_pn_select",
                               {0},
                               false,
                               0};
