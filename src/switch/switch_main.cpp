// Entry point Switch homebrew (libnx + EGL).
//
// Responsavel pelo bootstrap do hardware (libnx, HID), criacao do contexto
// GL via EGL e loop principal. A renderizacao em si fica em src/gfx_gl/
// (gl_renderer), agnostica de windowing — vai ser reutilizada pelo build
// POSIX desktop quando ele existir.

#include <cstdio>
#include <cstdlib>

#include <switch.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "gfx_gl/gl_renderer.h"

namespace {

EGLDisplay g_display = EGL_NO_DISPLAY;
EGLContext g_context = EGL_NO_CONTEXT;
EGLSurface g_surface = EGL_NO_SURFACE;

bool egl_init(NWindow *win)
{
    g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_display == EGL_NO_DISPLAY) {
        std::fprintf(stderr, "eglGetDisplay falhou: %d\n", eglGetError());
        return false;
    }

    eglInitialize(g_display, nullptr, nullptr);

    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE) {
        std::fprintf(stderr, "eglBindAPI(ES) falhou: %d\n", eglGetError());
        eglTerminate(g_display);
        g_display = EGL_NO_DISPLAY;
        return false;
    }

    static const EGLint cfg_attrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,    8,
        EGL_GREEN_SIZE,  8,
        EGL_BLUE_SIZE,   8,
        EGL_ALPHA_SIZE,  8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE,
    };
    EGLConfig config;
    EGLint num_configs = 0;
    eglChooseConfig(g_display, cfg_attrs, &config, 1, &num_configs);
    if (num_configs == 0) {
        std::fprintf(stderr, "eglChooseConfig falhou\n");
        eglTerminate(g_display);
        return false;
    }

    g_surface = eglCreateWindowSurface(g_display, config, win, nullptr);
    if (g_surface == EGL_NO_SURFACE) {
        std::fprintf(stderr, "eglCreateWindowSurface falhou: %d\n", eglGetError());
        eglTerminate(g_display);
        return false;
    }

    static const EGLint ctx_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };
    g_context = eglCreateContext(g_display, config, EGL_NO_CONTEXT, ctx_attrs);
    if (g_context == EGL_NO_CONTEXT) {
        std::fprintf(stderr, "eglCreateContext falhou: %d\n", eglGetError());
        eglDestroySurface(g_display, g_surface);
        eglTerminate(g_display);
        return false;
    }

    eglMakeCurrent(g_display, g_surface, g_surface, g_context);
    return true;
}

void egl_shutdown()
{
    if (g_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_context != EGL_NO_CONTEXT) {
            eglDestroyContext(g_display, g_context);
        }
        if (g_surface != EGL_NO_SURFACE) {
            eglDestroySurface(g_display, g_surface);
        }
        eglTerminate(g_display);
    }
}

} // namespace

int main(int /*argc*/, char ** /*argv*/)
{
    NWindow *win = nwindowGetDefault();
    if (!egl_init(win)) {
        return EXIT_FAILURE;
    }

    if (!gfx_gl::init()) {
        egl_shutdown();
        return EXIT_FAILURE;
    }
    gfx_gl::set_viewport(1280, 720);

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    const u64 start_tick = armGetSystemTick();

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
            break;
        }

        const float elapsed_s =
            float(armTicksToNs(armGetSystemTick() - start_tick)) * 1.0e-9f;

        gfx_gl::render_frame(elapsed_s);
        eglSwapBuffers(g_display, g_surface);
    }

    gfx_gl::shutdown();
    egl_shutdown();
    return EXIT_SUCCESS;
}
