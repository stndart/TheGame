#pragma once
#include "hook_manager.h"

extern HookStub g_target_w_connect_1;
extern HookStub g_target_w_connect_2;
extern HookStub g_target_w_connect_3;
extern HookStub g_target_ConvertWideToMultiByte;
extern HookStub g_target_EnsureWStringBufferCapacity;

// temp
LPSTR __cdecl EnsureWStringBufferCapacity(LPSTR *str_p, int required_bytes,
                                          LPSTR inline_buf,
                                          int inline_buf_size_bytes);

extern "C" {
void hook_w_connect_1();
void hook_w_connect_2();
void hook_w_connect_3();
void hook_ConvertWideToMultiByte();
void hook_EnsureWStringBufferCapacity();
}