#pragma once
#include "hook_manager.h"

extern HookStub g_target_w_connect_1;
extern HookStub g_target_w_connect_2;
extern HookStub g_target_w_connect_3;
extern HookStub g_target_ConvertWideToMultiByte;
extern HookStub g_target_EnsureWStringBufferCapacity;

// temp

extern "C" {
void hook_w_connect_1();
void hook_w_connect_2();
void hook_w_connect_3();
void hook_ConvertWideToMultiByte();
void hook_EnsureWStringBufferCapacity();
}