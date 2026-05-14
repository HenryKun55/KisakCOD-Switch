#pragma once

// Renderer GLES2 minimal, agnostico de windowing. O caller (switch_main.cpp
// no Switch, futuramente posix gl-main no desktop) e responsavel por criar
// o contexto GL antes de chamar gl_renderer_init.
//
// Estado atual: triangulo demo girando. API vai crescer pra incluir camera,
// buffers de vertice nomeados, texturas, materiais etc. — sem mudar o
// contrato basico de init/begin_frame/end_frame/shutdown.

namespace gfx_gl {

// Carrega shaders, cria program e VAO/VBO equivalentes. Requer um contexto
// GL ativo (eglMakeCurrent ja chamado).
bool init();

// Limpa o backbuffer e desenha o conteudo do frame atual. `time_seconds`
// vem do caller — o renderer nao mede tempo proprio pra deixar facil
// pausar/reescalar.
void render_frame(float time_seconds);

// Libera recursos GL. Deve ser chamado enquanto o contexto ainda esta
// ativo, antes do shutdown do windowing layer.
void shutdown();

// Atualiza o viewport quando a janela muda (handheld <-> docked no Switch,
// resize no desktop).
void set_viewport(int width, int height);

} // namespace gfx_gl
