// Entry point POSIX desktop com SDL2 + OpenGL — espelho do switch_main.cpp.
//
// Mesma logica de bootstrap: cria janela, contexto GL, delega o frame pro
// gfx_gl renderer. Permite iterar shaders e geometria sem ter que passar
// pelo emulador Switch (que demora ~5s pra carregar o .nro).
//
// macOS limita GL Compat profile a 2.1; pedimos isso pra manter
// compatibilidade com os shaders GLSL ES 1.00 do gl_renderer.cpp.
// Em Linux/outros desktop o mesmo perfil tambem funciona.

#include <cstdio>
#include <cstdlib>

#include <SDL.h>

// Em macOS o header GL fica em <OpenGL/gl.h>; em Linux/outros e <GL/gl.h>.
// SDL2 nao puxa nenhum dos dois automaticamente, mas o renderer
// (src/gfx_gl/gl_renderer.cpp) ja inclui <GLES2/gl2.h> que via SDL_opengl.h
// resolve as funcoes em runtime — entao aqui so precisamos do SDL.

#include "gfx_gl/gl_renderer.h"

namespace {

constexpr int WINDOW_W = 1280;
constexpr int WINDOW_H = 720;

} // namespace

int main(int /*argc*/, char ** /*argv*/)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init falhou: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *window = SDL_CreateWindow(
        "KisakCOD-Switch (POSIX)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);

    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow falhou: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (!ctx) {
        std::fprintf(stderr, "SDL_GL_CreateContext falhou: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    SDL_GL_MakeCurrent(window, ctx);
    SDL_GL_SetSwapInterval(1); // vsync

    int draw_w = WINDOW_W;
    int draw_h = WINDOW_H;
    SDL_GL_GetDrawableSize(window, &draw_w, &draw_h);

    if (!gfx_gl::init()) {
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    gfx_gl::set_viewport(draw_w, draw_h);

    const Uint64 start_ms = SDL_GetTicks64();
    bool running = true;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
            } else if (ev.type == SDL_KEYDOWN &&
                       ev.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            } else if (ev.type == SDL_WINDOWEVENT &&
                       ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                SDL_GL_GetDrawableSize(window, &draw_w, &draw_h);
                gfx_gl::set_viewport(draw_w, draw_h);
            }
        }

        const float elapsed_s = float(SDL_GetTicks64() - start_ms) * 1.0e-3f;
        gfx_gl::render_frame(elapsed_s);
        SDL_GL_SwapWindow(window);
    }

    gfx_gl::shutdown();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
