#pragma once
#include "hook_manager.h"

extern HookStub g_target_w_connect_1;
extern HookStub g_target_w_connect_2;
extern HookStub g_target_w_connect_3;

extern HookStub g_target_EnsureMStringBufferCapacity;
extern HookStub g_target_EnsureWStringBufferCapacity;
extern HookStub g_target_ConvertWideToMultiByte;
extern HookStub g_target_ConvertMultiByteToWide;

extern HookStub g_target_rstring_truncate;
extern HookStub g_target_rstring_realloc;
extern HookStub g_target_rstring_reserve;

extern HookStub g_target_w_strlen;

// temp

extern "C" {
void hook_w_connect_1();
void hook_w_connect_2();
void hook_w_connect_3();

void hook_EnsureMStringBufferCapacity();
void hook_EnsureWStringBufferCapacity();
void hook_ConvertWideToMultiByte();
void hook_ConvertMultiByteToWide();

void hook_rstring_truncate();
void hook_rstring_realloc();
void hook_rstring_reserve();

void hook_w_strlen();
}