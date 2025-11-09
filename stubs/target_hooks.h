#pragma once
#include "hook_manager.h"

// Example stub definitions - adjust RVAs to match your target
extern HookStub g_target_example_method;
extern HookStub g_another_target_method;

// Declare all your hook functions with proper calling conventions
extern "C" {
void __fastcall hook_example_method();
void __fastcall hook_another_target_method();
}