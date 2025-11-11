#include "console.h"
#include "crt/memory.h"
#include "target_hooks.h"

#include <atlexcept.h>
#include <errhandlingapi.h>
#include <winnt.h>
#include <winsvc.h>

LPSTR __cdecl EnsureWStringBufferCapacity(LPSTR *str_p, int required_bytes,
                                          LPSTR inline_buf,
                                          int inline_buf_size_bytes) {
  logf("EnsureWStringBufferCapacity: required_bytes='%i' with inline size='%i'",
       required_bytes, inline_buf_size_bytes);

  if (!str_p || (required_bytes < 0) || !inline_buf)
    throw ATL::CAtlException(E_INVALIDARG);

  if (*str_p != inline_buf) {                     // if using heap
    if (required_bytes > inline_buf_size_bytes) { // reallocate on heap
      logf("recalloc at %p of %u", *str_p, required_bytes);
      *str_p = reinterpret_cast<LPSTR>(
          CRT::recalloc(reinterpret_cast<void *>(*str_p), required_bytes, 1));
      if (!*str_p)
        throw ATL::CAtlException(E_OUTOFMEMORY);
    } else { // use stack buf, since it's sufficient
      logf("free at %p and go to heap %p", *str_p, inline_buf);
      CRT::free(*str_p);
      *str_p = inline_buf;
    }
  } else { // str_p is pointing at stack_buf
    if (required_bytes > inline_buf_size_bytes) { // use heap only if needed
      *str_p = reinterpret_cast<LPSTR>(CRT::calloc(required_bytes, 1));
      logf("recalloc at %p of %u", *str_p, required_bytes);
      if (!*str_p)
        throw ATL::CAtlException(E_OUTOFMEMORY);
    }
  }

  return *str_p;
}

extern "C" void __declspec(naked) hook_EnsureWStringBufferCapacity() {
  __asm {
    // EAX, ECX, EDX, EBX, ESP (original value), EBP, ESI, and EDI
    pushad
        // push ebx
        // push esi
        // push edi

    mov eax, [esp + 0x24] ; // inline_buf_size_bytes
    mov ebx, [esp + 0x28] ; // inline_buf 
    mov ecx, [esp + 0x2C] ; // required_bytes
    mov edx, [esp + 0x30] ; // str_p
    push edx
    push ecx
    push ebx
    push eax

    call EnsureWStringBufferCapacity

    add esp, 16

    mov [esp + 0x1C], eax
    mov [esp + 0x18], ecx
    mov [esp + 0x14], edx
    popad

        ret

            // Execute original instructions that were overwritten and jump back
    push esi
    mov esi, [esp+8]
    jmp [g_target_EnsureWStringBufferCapacity.return_addr]
  }
}

HookStub g_target_EnsureWStringBufferCapacity = {
    0x5584C0,
    (uint32_t)(uintptr_t)hook_EnsureWStringBufferCapacity,
    "hook_EnsureWStringBufferCapacity",
    {0},
    false,
    0x5584C5,
};