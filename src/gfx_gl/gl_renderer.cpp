// Implementacao do renderer GLES2 minimal — ver gl_renderer.h.

#include "gl_renderer.h"

#include <cstdio>

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
    "attribute vec2 a_pos;\n"
    "attribute vec3 a_col;\n"
    "uniform float u_time;\n"
    "varying vec3 v_col;\n"
    "void main() {\n"
    "    float c = cos(u_time);\n"
    "    float s = sin(u_time);\n"
    "    vec2 rot = vec2(c * a_pos.x - s * a_pos.y,\n"
    "                    s * a_pos.x + c * a_pos.y);\n"
    "    v_col = a_col;\n"
    "    gl_Position = vec4(rot, 0.0, 1.0);\n"
    "}\n";

constexpr const char *FRAGMENT_SRC =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec3 v_col;\n"
    "void main() {\n"
    "    gl_FragColor = vec4(v_col, 1.0);\n"
    "}\n";

// Geometria estatica por enquanto. Quando o renderer crescer essa lista
// vai sair daqui pra um sistema de meshes carregados.
constexpr GLfloat TRIANGLE[] = {
//   x      y      r     g     b
     0.0f,  0.7f,  1.0f, 0.2f, 0.2f,
    -0.7f, -0.5f,  0.2f, 1.0f, 0.2f,
     0.7f, -0.5f,  0.2f, 0.4f, 1.0f,
};

GLuint g_program  = 0;
GLint  g_u_time   = -1;

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
    g_u_time = glGetUniformLocation(g_program, "u_time");

    glClearColor(0.10f, 0.12f, 0.15f, 1.0f);
    return true;
}

void set_viewport(int width, int height)
{
    glViewport(0, 0, width, height);
}

void render_frame(float time_seconds)
{
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_program);
    glUniform1f(g_u_time, time_seconds);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), TRIANGLE);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), TRIANGLE + 2);
    glDrawArrays(GL_TRIANGLES, 0, 3);
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
