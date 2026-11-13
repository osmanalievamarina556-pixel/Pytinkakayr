// inline_hook.h: arm64 inline hook без сторонних библиотек
// Не проверено на устройстве. Риск: первые 16 байт цели должны быть не PC-relative.
#pragma once
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>

// LDR X16, #8  ; BR X16 ; .quad addr  — 16 байт
static inline bool inline_hook(void* target, void* repl, void** orig) {
    const size_t N = 16;
    const size_t pg = (size_t)getpagesize();

    // трамплин: исходные 16 байт + переход обратно на target+16
    uint8_t* tramp = (uint8_t*)mmap(nullptr, 64, PROT_READ | PROT_WRITE | PROT_EXEC,
                                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (tramp == MAP_FAILED) return false;
    memcpy(tramp, target, N);
    uint32_t* t = (uint32_t*)(tramp + N);
    t[0] = 0x58000050;   // LDR X16, #8
    t[1] = 0xD61F0200;   // BR X16
    *(uint64_t*)&t[2] = (uint64_t)((uint8_t*)target + N);
    *orig = tramp;

    // патч цели
    uintptr_t page = (uintptr_t)target & ~(uintptr_t)(pg - 1);
    if (mprotect((void*)page, pg * 2, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
        return false;
    uint32_t* p = (uint32_t*)target;
    p[0] = 0x58000050;
    p[1] = 0xD61F0200;
    *(uint64_t*)&p[2] = (uint64_t)repl;
    __builtin___clear_cache((char*)target, (char*)target + N);
    return true;
}
