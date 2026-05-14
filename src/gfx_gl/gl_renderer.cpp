// Implementacao do renderer GLES2 minimal — ver gl_renderer.h.

#include "gl_renderer.h"

#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// GL header conditional: GLES2 no Switch/Android, Apple OpenGL framework
// no macOS (legacy 2.1 Compat suficiente pras funcoes que usamos), Linux
// usa o header generic. Todas as funcoes que tocamos
// (glCreateShader/.../glDrawArrays) sao GL 2.0+ core OU GLES 2.0.
#if defined(__SWITCH__)
    #include <GLES2/gl2.h>
#elif defined(__APPLE__)
    #define GL_SILENCE_DEPRECATION
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif

namespace gfx_gl {

namespace {

// Compativel com GLSL ES 1.00 (Switch via mesa-nouveau) e GLSL 1.20
// (macOS OpenGL 2.1 Compat). O qualificador `precision` so existe em
// GLSL ES — o `#ifdef GL_ES` e parseado pelo compilador GLSL.
constexpr const char *VERTEX_SRC =
    "attribute vec3 a_pos;\n"
    "attribute vec3 a_col;\n"
    "uniform mat4 u_mvp;\n"
    "varying vec3 v_col;\n"
    "void main() {\n"
    "    v_col = a_col;\n"
    "    gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
    "}\n";

constexpr const char *FRAGMENT_SRC =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec3 v_col;\n"
    "void main() {\n"
    "    gl_FragColor = vec4(v_col, 1.0);\n"
    "}\n";

// Cubo unitario centrado na origem. Cada vertice tem cor distinta — vai
// dar gradiente nas faces.
constexpr GLfloat CUBE_VERTS[] = {
//   x      y      z      r     g     b
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 0.0f, // 0: traseira-inferior-esq, preto
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f, // 1: traseira-inferior-dir, vermelho
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f, // 2: traseira-superior-dir, amarelo
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f, // 3: traseira-superior-esq, verde
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f, // 4: frontal-inferior-esq, azul
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f, // 5: frontal-inferior-dir, magenta
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f, // 6: frontal-superior-dir, branco
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f, // 7: frontal-superior-esq, ciano
};

constexpr GLushort CUBE_INDICES[] = {
    // 12 triangulos (2 por face, 6 faces). Ordem CCW olhando de fora.
    4, 5, 6,  4, 6, 7, // frontal
    1, 0, 3,  1, 3, 2, // traseira
    0, 4, 7,  0, 7, 3, // esquerda
    5, 1, 2,  5, 2, 6, // direita
    3, 7, 6,  3, 6, 2, // topo
    0, 1, 5,  0, 5, 4, // base
};

GLuint g_program       = 0;
GLint  g_u_mvp         = -1;
int    g_viewport_w    = 1280;
int    g_viewport_h    = 720;

GLuint compile_shader(GLenum type, const char *src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512] = {};
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[gfx_gl] shader compile error: %s\n", log);
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
        char log[512] = {};
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[gfx_gl] program link error: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

} // namespace

bool init()
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, VERTEX_SRC);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SRC);
    if (!vs || !fs) {
        return false;
    }
    g_program = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!g_program) {
        return false;
    }
    g_u_mvp = glGetUniformLocation(g_program, "u_mvp");

    glClearColor(0.10f, 0.12f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    return true;
}

void set_viewport(int width, int height)
{
    g_viewport_w = width;
    g_viewport_h = height;
    glViewport(0, 0, width, height);
}

void render_frame(float time_seconds)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float aspect = float(g_viewport_w) / float(g_viewport_h);
    const glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
    const glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 3.0f),  // camera 3 unidades atras
        glm::vec3(0.0f, 0.0f, 0.0f),  // olhando pra origem
        glm::vec3(0.0f, 1.0f, 0.0f)); // up = +Y
    glm::mat4 model(1.0f);
    model = glm::rotate(model, time_seconds * 0.7f, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, time_seconds * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::mat4 mvp = proj * view * model;

    glUseProgram(g_program);
    glUniformMatrix4fv(g_u_mvp, 1, GL_FALSE, &mvp[0][0]);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), CUBE_VERTS);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), CUBE_VERTS + 3);
    glDrawElements(GL_TRIANGLES,
                   sizeof(CUBE_INDICES) / sizeof(CUBE_INDICES[0]),
                   GL_UNSIGNED_SHORT,
                   CUBE_INDICES);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
}

void shutdown()
{
    if (g_program) {
        glDeleteProgram(g_program);
        g_program = 0;
    }
}

} // namespace gfx_gl
