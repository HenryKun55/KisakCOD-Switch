// Shims de compatibilidade MSVC para builds POSIX/Switch.
//
// Force-incluido antes de qualquer .cpp/.h via flag -include do compilador
// (ver scripts/posix/CMakeLists.txt). Permite que codigo upstream que usa
// keywords e tipos MSVC (__cdecl, __declspec, __int16, etc.) compile em
// clang/gcc sem alteracoes invasivas no source.
//
// Mantemos o cabecalho minimalista: so o que aparece de fato no codigo
// upstream do KisakCOD. Quando um shim novo for necessario, adicionar aqui
// em vez de ifdef'ar o site de uso.

#pragma once

#if !defined(_MSC_VER)

// === Convencoes de chamada ===================================================
// MSVC usa __cdecl/__stdcall/__fastcall pra controlar como argumentos sao
// empilhados. Em ARM64 e x86_64 nao-Windows, AAPCS/SysV ABI sao impostas
// pelo compilador independente desses keywords — viram no-op.
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __fastcall
#define __fastcall
#endif

// === Forced inline ===========================================================
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif

// === __declspec(...) =========================================================
// Usado para visibilidade DLL, alinhamento, thread-local, etc. Na pratica do
// upstream do KisakCOD aparece como __declspec(noreturn) e __declspec(align).
// No-op generico cobre os usos atuais; refinar se algum site quebrar.
#ifndef __declspec
#define __declspec(x)
#endif

// === Tipos inteiros de tamanho fixo ==========================================
// MSVC tem __int8/16/32/64 como builtins. Em POSIX vem de <cstdint>.
#include <cstdint>
typedef int8_t   __int8;
typedef int16_t  __int16;
typedef int32_t  __int32;
typedef int64_t  __int64;

// === __pragma ================================================================
// Versao function-like do #pragma usada em MSVC pra meter pragma dentro de
// macros. Em clang/gcc o equivalente seria _Pragma() — por enquanto no-op
// porque os usos no upstream sao quase todos warning-disables que ja sao
// tratados pelos flags do build POSIX.
#ifndef __pragma
#define __pragma(x)
#endif

#endif // !_MSC_VER
