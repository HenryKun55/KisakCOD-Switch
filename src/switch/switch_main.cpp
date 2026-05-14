// Entry point Switch homebrew (libnx + EGL + GLES2).
//
// Substitui src/posix/posix_main.cpp no build do target Switch — ver
// scripts/switch/CMakeLists.txt. Por enquanto so inicializa um contexto
// GLES2 e renderiza um triangulo colorido na tela como prova de pipeline
// grafico; e a primeira pedra do futuro renderer que substituira
// src/gfx_d3d/ do upstream.
//
// Quando o renderer real existir, este arquivo so faz o bootstrap do libnx
// e delega pra um `gl_renderer_*` em src/gfx_gl/.

#include <cstdio>
#include <cstdlib>

#include <switch.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

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

// ---- Shader pipeline minimo --------------------------------------------------

constexpr const char *VERTEX_SRC =
    "attribute vec2 a_pos;\n"
    "attribute vec3 a_col;\n"
    "varying vec3 v_col;\n"
    "void main() {\n"
    "    v_col = a_col;\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

constexpr const char *FRAGMENT_SRC =
    "precision mediump float;\n"
    "varying vec3 v_col;\n"
    "void main() {\n"
    "    gl_FragColor = vec4(v_col, 1.0);\n"
    "}\n";

GLuint compile_shader(GLenum type, const char *src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        std::fprintf(stderr, "shader compile error: %s\n", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

GLuint link_program(GLuint vs, GLuint fs)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "a_pos");
    glBindAttribLocation(p, 1, "a_col");
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::fprintf(stderr, "program link error: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

} // namespace

int main(int /*argc*/, char ** /*argv*/)
{
    NWindow *win = nwindowGetDefault();
    if (!egl_init(win)) {
        return EXIT_FAILURE;
    }

    GLuint vs = compile_shader(GL_VERTEX_SHADER, VERTEX_SRC);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SRC);
    GLuint prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // Triangulo: posicao (xy) + cor (rgb), no espaco clip [-1, 1].
    const GLfloat verts[] = {
    //   x      y      r     g     b
         0.0f,  0.7f,  1.0f, 0.2f, 0.2f,  // topo, vermelho
        -0.7f, -0.5f,  0.2f, 1.0f, 0.2f,  // esquerda, verde
         0.7f, -0.5f,  0.2f, 0.4f, 1.0f,  // direita, azul
    };

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    glViewport(0, 0, 1280, 720);
    glClearColor(0.10f, 0.12f, 0.15f, 1.0f);

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
            break;
        }

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), verts);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), verts + 2);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);

        eglSwapBuffers(g_display, g_surface);
    }

    glDeleteProgram(prog);
    egl_shutdown();
    return EXIT_SUCCESS;
}
