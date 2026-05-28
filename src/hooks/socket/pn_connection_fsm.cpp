#include "target_hooks.h"

#include "game/net/pn_connection.hpp"

extern "C" char __stdcall hook_pn_fsm_state1(int conn) {
  return reinterpret_cast<PNConnectionNode *>(conn)->run_state_1();
}

extern "C" char __stdcall hook_pn_fsm_state2(int conn) {
  return reinterpret_cast<PNConnectionNode *>(conn)->run_state_2();
}

extern "C" int __stdcall hook_pn_fsm_state3(int conn) {
  return reinterpret_cast<PNConnectionNode *>(conn)->run_state_3();
}

HookStub g_target_pn_fsm_state1 = {pn::rva::kFsmState1,
                                   (uint32_t)(uintptr_t)hook_pn_fsm_state1,
                                   "hook_pn_fsm_state1",
                                   {0},
                                   false,
                                   0};

HookStub g_target_pn_fsm_state2 = {pn::rva::kFsmState2,
                                   (uint32_t)(uintptr_t)hook_pn_fsm_state2,
                                   "hook_pn_fsm_state2",
                                   {0},
                                   false,
                                   0};

HookStub g_target_pn_fsm_state3 = {pn::rva::kFsmState3,
                                   (uint32_t)(uintptr_t)hook_pn_fsm_state3,
                                   "hook_pn_fsm_state3",
                                   {0},
                                   false,
                                   0};
