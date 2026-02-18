#include <GLES2/gl2.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <cmath>
#include <cstdio>

// --- Global Variables ---
EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
GLuint program;
GLuint vertexBuffer;
GLint angleLocation;
float angle = 0.0f;

// Triangle vertices (2D)
float vertices[] = {
     0.0f,  0.5f,
    -0.5f, -0.5f,
     0.5f, -0.5f
};

// --- Shader Sources ---
const char* vertexShaderSource = R"(
attribute vec2 aPosition;
uniform float uAngle;
void main() {
    float cosA = cos(uAngle);
    float sinA = sin(uAngle);
    gl_Position = vec4(
        aPosition.x * cosA - aPosition.y * sinA,
        aPosition.x * sinA + aPosition.y * cosA,
        0.0,
        1.0
    );
}
)";

const char* fragmentShaderSource = R"(
precision mediump float;
void main() {
    gl_FragColor = vec4(1.0, 0.5, 0.0, 1.0); // Orange
}
)";

// --- Shader Compilation ---
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        emscripten_log(EM_LOG_ERROR, "Shader compile error: %s", info);
    }
    return shader;
}

// --- Initialization ---
void init() {
    // Set canvas size
    emscripten_set_canvas_element_size("#canvas", 800, 600);

    // Create WebGL2 context
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.alpha = EM_TRUE;
    attr.depth = EM_TRUE;
    attr.stencil = EM_FALSE;
    attr.antialias = EM_TRUE;
    attr.majorVersion = 2; // WebGL2
    attr.minorVersion = 0;

    ctx = emscripten_webgl_create_context("#canvas", &attr);
    if (ctx <= 0) {
        emscripten_log(EM_LOG_ERROR, "Failed to create WebGL2 context!");
        return;
    }

    emscripten_webgl_make_context_current(ctx);

    // Compile shaders and link program
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(program, 512, nullptr, info);
        emscripten_log(EM_LOG_ERROR, "Program link error: %s", info);
    }

    glUseProgram(program);

    // Setup vertex buffer
    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GLint posAttrib = glGetAttribLocation(program, "aPosition");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

    angleLocation = glGetUniformLocation(program, "uAngle");
}

// --- Render Loop ---
void loop() {
    angle += 0.02f;

    glViewport(0, 0, 800, 600);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(program);
    glUniform1f(angleLocation, angle);

    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// --- Main Entry ---
int main() {
    init();
    emscripten_set_main_loop(loop, 0, 1);
    return 0;
}
