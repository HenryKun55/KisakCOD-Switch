// Entry point Switch homebrew (libnx + EGL).
//
// Responsavel pelo bootstrap do hardware (libnx, HID), criacao do contexto
// GL via EGL e loop principal. A renderizacao em si fica em src/gfx_gl/
// (gl_renderer), agnostica de windowing — vai ser reutilizada pelo build
// POSIX desktop quando ele existir.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <switch.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "gfx_gl/gl_renderer.h"
#include "qcommon/qcommon.h"
#include "client_mp/client_mp.h"

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

// Route Sys_Print (the upstream CoD4 console output sink) to the Switch
// debug log. On Ryujinx this surfaces as `Application LogInfo: ...` lines,
// which is the only way we can see Com_Printf / Com_PrintError during
// Com_Init when no console framebuffer is attached.
void switch_debug_log(const char *msg)
{
    if (!msg) return;
    svcOutputDebugString(msg, std::strlen(msg));
}

} // namespace

void Sys_Print(const char *msg)
{
    switch_debug_log(msg);
}

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

    // Upstream's D3D9 init populates cls.vidConfig when the device is
    // created; on Switch our renderer is separate so we seed it before
    // Com_Init runs the client side, which asserts displayWidth > 0.
    cls.vidConfig.sceneWidth = 1280;
    cls.vidConfig.sceneHeight = 720;
    cls.vidConfig.displayWidth = 1280;
    cls.vidConfig.displayHeight = 720;
    cls.vidConfig.displayFrequency = 60;
    cls.vidConfig.isFullscreen = 1;
    cls.vidConfig.aspectRatioWindow = 16.0f / 9.0f;
    cls.vidConfig.aspectRatioScenePixel = 1.0f;
    cls.vidConfig.aspectRatioDisplayPixel = 1.0f;
    cls.vidConfig.maxTextureSize = 4096;
    cls.vidConfig.maxTextureMaps = 8;
    cls.vidConfig.deviceSupportsGamma = false;

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    // Anchor the engine FS root at sdmc:/switch/cod4/ so CoD4's relative
    // paths (./main/iw_00.iwd, ./zone/english/*.ff, ...) resolve into the
    // game assets we drop alongside the NRO on the SD card.
    if (chdir("sdmc:/switch/cod4") != 0) {
        switch_debug_log("[switch_main] chdir(sdmc:/switch/cod4) failed\n");
    } else {
        switch_debug_log("[switch_main] chdir(sdmc:/switch/cod4) OK\n");
    }

    switch_debug_log("[switch_main] before Com_InitThreadData\n");
    // Bring up the CoD4 engine. Com_InitThreadData(0) seeds the main thread's
    // TLS slots (jmp_buf at slot 2, va rotating buffer at slot 1) which
    // Com_Init dereferences immediately via Sys_GetValue(2). Upstream this
    // happens inside Sys_InitMainThread() — that one is Win32-only, so we
    // call the cross-platform half directly.
    Com_InitThreadData(0);
    switch_debug_log("[switch_main] after Com_InitThreadData, calling Com_Init\n");

    // Com_Init pulls in FS, dvars, hunk alloc, localization, etc. If iwd
    // assets are missing this raises Sys_Error and exits; the demo cube
    // path below is only reached when init returns.
    char cmdline[1] = {0};
    Com_Init(cmdline);
    extern void switch_reset_render_queue();
    extern void switch_dispatch_render_queue();

    switch_debug_log("[switch_main] Com_Init returned, entering Com_Frame loop\n");

    int frame = 0;
    while (appletMainLoop()) {
        switch_reset_render_queue();
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
            break;
        }

        if (frame < 5) {
            char dbg[64];
            std::snprintf(dbg, sizeof(dbg), "[switch_main] Com_Frame begin #%d\n", frame);
            switch_debug_log(dbg);
        }
        Com_Frame();
        if (frame < 5) {
            char dbg[64];
            std::snprintf(dbg, sizeof(dbg), "[switch_main] Com_Frame end #%d\n", frame);
            switch_debug_log(dbg);
        }
        ++frame;

        // Apply CoD4's render queue (RC_CLEAR_SCREEN feeds glClearColor)
        // and present. No more demo cube on top — the framebuffer is
        // entirely engine-driven now. As more RC_* opcodes get wired
        // into switch_dispatch_render_queue, real CoD4 pixels start
        // appearing on this same swap chain.
        switch_dispatch_render_queue();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        eglSwapBuffers(g_display, g_surface);
    }

    gfx_gl::shutdown();
    egl_shutdown();
    return EXIT_SUCCESS;
}
