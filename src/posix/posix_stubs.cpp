// Stubs de simbolos upstream referenciados pelos primeiros arquivos
// portados, mas cujas definicoes vivem em fontes que ainda nao compilam
// no POSIX (xanim/ode, qcommon/threads.h com Windows.h, etc.).
//
// Cada stub aqui e provisorio: imprime uma marca [stub] no stderr quando
// chamado em runtime, retorna um valor neutro, e *deve ser removido* assim
// que o arquivo dono for portado de verdade. Veja docs/SWITCH_PORT.md.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <strings.h> // strcasecmp

// I_stricmp: declarado em universal/q_shared.h, definido em q_shared.cpp.
// Equivalente POSIX direto.
int I_stricmp(const char *s0, const char *s1)
{
    return ::strcasecmp(s0 ? s0 : "", s1 ? s1 : "");
}

// AxisToQuat: declarado em universal/com_math.h (linha 292), definido em
// com_math.cpp (que ainda nao compila no POSIX por causa de xanim/ode).
// Stub retorna quaternion identidade.
void AxisToQuat(const float (*mat)[3], float *out)
{
    (void)mat;
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;
    out[3] = 1.0f;
    static bool warned = false;
    if (!warned) {
        std::fprintf(stderr, "[stub] AxisToQuat: identidade — porte de com_math.cpp pendente\n");
        warned = true;
    }
}

// Vec2Normalize: declarado em universal/com_math.h (linha 230), definido em
// com_math.cpp linha 559. Stub computa o normalize na mao (sem usar vec2r
// pra evitar arrastar o header todo) — base correta pra remover quando
// com_math.cpp portar.
float Vec2Normalize(float *v)
{
    const float lensq = v[0] * v[0] + v[1] * v[1];
    if (lensq <= 0.0f) {
        return 0.0f;
    }
    const float len = std::sqrt(lensq);
    const float inv = 1.0f / len;
    v[0] *= inv;
    v[1] *= inv;
    return len;
}
