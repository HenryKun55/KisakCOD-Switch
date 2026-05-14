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

// === Headers padrao que upstream assume sem includes explicitos =============
// q_shared.h usa INT_MIN/INT_MAX em DvarLimits sem incluir <limits.h>. O
// build MSVC pega isso transitivamente de algum outro header da MS CRT.
// Aqui forcamos disponibilidade.
#include <climits>

// === Colisao com libc POSIX: random()/srandom() =============================
// <stdlib.h> em POSIX declara `long random(void)` (BSD-derivada). CoD4 tem
// sua propria `float random()` em com_math.h, o que vira "differ only in
// return type" e quebra a compilacao. Solucao: rename CoD4's `random`/`crandom`
// via macros aplicadas APOS <cstdlib> ter sido visto. A funcao da libc
// continua acessivel pelo simbolo `random`; codigo CoD4 vira `kisak_random`.
#include <cstdlib>
#define random  kisak_random
#define crandom kisak_crandom

// === Tipos inteiros de tamanho fixo ==========================================
// MSVC tem __int8/16/32/64 como builtins, o que permite `unsigned __int8`,
// `signed __int8`, etc. Em POSIX usamos #define (nao typedef!) para preservar
// essa propriedade — o preprocessador troca o token cedo, deixando `unsigned`
// se combinar com o tipo basico. typedef quebraria com `unsigned __int8 x`.
//
// Nota: __int8 mapeado pra `char` (plain) tem sinal implementation-defined
// no padrao, enquanto MSVC garante signed. Os usos no upstream sao quase
// todos via `unsigned __int8` (bytes), entao a diferenca raramente aparece.
#define __int8  char
#define __int16 short
#define __int32 int
#define __int64 long long

// === __pragma ================================================================
// Versao function-like do #pragma usada em MSVC pra meter pragma dentro de
// macros. Em clang/gcc o equivalente seria _Pragma() — por enquanto no-op
// porque os usos no upstream sao quase todos warning-disables que ja sao
// tratados pelos flags do build POSIX.
#ifndef __pragma
#define __pragma(x)
#endif

#endif // !_MSC_VER
