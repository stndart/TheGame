#pragma once
#include "hook_manager.h"

void __cdecl log_str(void **handle);

extern HookStub g_target_w_connect_1;
extern HookStub g_target_w_connect_2;
extern HookStub g_target_w_connect_3;

extern HookStub g_target_EnsureMStringBufferCapacity;
extern HookStub g_target_EnsureWStringBufferCapacity;
extern HookStub g_target_ConvertWideToMultiByte;
extern HookStub g_target_ConvertMultiByteToWide;

extern HookStub g_target_rstring_truncate;
extern HookStub g_target_rstring_truncate_self;
extern HookStub g_target_rstring_decrefcnt;
extern HookStub g_target_rstring_copyonwrite;
extern HookStub g_target_rstring_realloc;
extern HookStub g_target_rstring_reserve;

extern HookStub g_target_rstring_truncateatfirst;
extern HookStub g_target_rstring_trimleft;
extern HookStub g_target_rstring_concatenate;
extern HookStub g_target_rstring_concatenate_cstr;

extern HookStub g_target_w_strlen;
