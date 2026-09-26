cmake_minimum_required(VERSION 3.20)
project(poe2_internal CXX ASM_MASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Compile flags (MSVC x64) ──────────────────────────────────────────────────
if(MSVC)
    # /GS-  → no security cookies (нет CRT в payload)
    # /EHa  → SEH exceptions (__try/__except)
    # /MT   → static CRT linkage
    # /O2   → optimize
    # /GR-  → no RTTI
    add_compile_options(/GS- /EHa /MT /O2 /GR- /W3 /WX-)
    # x64 only
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/arch:AVX2>")
endif()

set(COMMON_INC "${CMAKE_SOURCE_DIR}/common")

# ── MASM: syscall stub ────────────────────────────────────────────────────────
enable_language(ASM_MASM)
set(SYSCALL_STUB "${CMAKE_SOURCE_DIR}/loader/syscall_stub.asm")
set_source_files_properties(${SYSCALL_STUB} PROPERTIES LANGUAGE ASM_MASM)

# ── poe2_loader.exe ───────────────────────────────────────────────────────────
add_executable(poe2_loader
    loader/mapper.cpp
    ${SYSCALL_STUB}
)
target_include_directories(poe2_loader PRIVATE ${COMMON_INC})
target_link_libraries(poe2_loader PRIVATE ntdll.lib)
set_target_properties(poe2_loader PROPERTIES
    OUTPUT_NAME "poe2_loader"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
)

# ── poe2_payload.dll ──────────────────────────────────────────────────────────
add_library(poe2_payload SHARED
    internal/payload.cpp
)
target_include_directories(poe2_payload PRIVATE ${COMMON_INC})
# Нет CRT linkage у payload
target_compile_options(poe2_payload PRIVATE /GS- /MT /O2 /EHa /GR-)
# Нет дефолтных lib — только то, что нужно
target_link_options(poe2_payload PRIVATE
    /NODEFAULTLIB
    /ENTRY:DllMain
)
target_link_libraries(poe2_payload PRIVATE ntdll.lib kernel32.lib)
set_target_properties(poe2_payload PROPERTIES
    OUTPUT_NAME "poe2_payload"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
    # Нет .exp/.lib в output
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
)
