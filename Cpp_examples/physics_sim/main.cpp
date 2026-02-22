#include <emscripten/emscripten.h>
#include <GLES2/gl2.h>
#include <emscripten/html5.h>
#include <cmath>
#include <vector>

EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
GLuint program;
GLuint vertexBuffer;
GLint colorLocation;

// --- Circle config ---
const int CIRCLE_SEGMENTS = 50;
const float BALL_RADIUS = 0.02f; // fraction of canvas (NDC)
class Ball {
public:
    Ball(float x, float y, float vx, float vy, float r, float g, float b)
        : x(x), y(y), vx(vx), vy(vy), r(r), g(g), b(b) {}
    float x, y;
    float vx, vy;
    float r, g, b;
};

std::vector<Ball> balls;
bool pauseSimulation = false;
bool isGravity = false;
bool isElastic = false;

// Function to toggle pause
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    bool togglePause() {
        return pauseSimulation = !pauseSimulation;
    }

    EMSCRIPTEN_KEEPALIVE
    bool toggleGravity()
    {
        return isGravity = !isGravity;
    }
    EMSCRIPTEN_KEEPALIVE
    bool toggleElastic()
    {
        return isElastic = !isElastic;
    }
}

// --- Shader sources ---
const char* vertexShaderSource = R"(
attribute vec2 aPosition;
void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)";
const char* fragmentShaderSource = R"(
precision mediump float;
uniform vec4 uColor;
void main() {
    gl_FragColor = uColor;
}
)";

// --- Compile shader utility ---
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

// --- Vertex generation with aspect correction ---
void createCircleVertices(float cx, float cy, float radius, float* outVertices) {
    double cssWidth, cssHeight;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);
    float aspect = (float)cssWidth / (float)cssHeight;

    outVertices[0] = cx;
    outVertices[1] = cy;
    for (int i = 0; i <= CIRCLE_SEGMENTS; ++i) {
        float angle = 2.0f * M_PI * i / CIRCLE_SEGMENTS;
        float dx = cosf(angle) * radius;
        float dy = sinf(angle) * radius;

        if (cssWidth > cssHeight) dx /= aspect;
        else dy *= aspect;

        outVertices[2 + i * 2] = cx + dx;
        outVertices[2 + i * 2 + 1] = cy + dy;
    }
}

// --- Canvas resize ---
void resizeCanvas() {
    double cssWidth, cssHeight;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);
    double dpr = emscripten_get_device_pixel_ratio();
    emscripten_set_canvas_element_size("#canvas", (int)(cssWidth * dpr), (int)(cssHeight * dpr));
    glViewport(0, 0, (int)(cssWidth * dpr), (int)(cssHeight * dpr));
}

// --- Mouse click handler ---
EM_BOOL onMouseClick(int eventType, const EmscriptenMouseEvent* e, void* userData) {
    int w, h;
    emscripten_get_canvas_element_size("#canvas", &w, &h);
    double dpr = emscripten_get_device_pixel_ratio();

    float xNDC = (float)((e->targetX * dpr / w) * 2.0 - 1.0);
    float yNDC = (float)(1.0 - (e->targetY * dpr / h) * 2.0);

    // Random velocity for fun
    float vx = ((rand() % 200) / 10000.0f) - 0.005f; // -0.1 to 0.1
    float vy = ((rand() % 200) / 10000.0f) - 0.005f;

    balls.push_back({ xNDC, yNDC, vx, vy, 1.0f, 0.0f, 0.0f }); // red ball
    return EM_TRUE;
}

// --- Update physics ---
void updatePhysics(float dt) {
    double cssWidth, cssHeight;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);
    float aspect = (float)cssWidth / (float)cssHeight;

    float bounceFactor = isElastic ? 1.0f : 0.95f;

    float subDt = 0.005f; // 5ms substeps
    int steps = (int)(dt / subDt) + 1;
    float realDt = dt / steps;

    for (int s = 0; s < steps; s++) {
        for (auto &b : balls) {
            // Apply gravity
            if(isGravity) b.vy += -0.00035f * realDt;

            // Update positions
            b.x += b.vx * realDt;
            b.y += b.vy * realDt;

            // Adjust radius for aspect ratio
            float radiusX = BALL_RADIUS;
            float radiusY = BALL_RADIUS;
            if (cssWidth > cssHeight) radiusX /= aspect;
            else radiusY *= aspect;

            // Vertical bounce
            if (b.y + radiusY > 1.0f) {
                b.y = 1.0f - radiusY;
                b.vy *= -bounceFactor;
            } else if (b.y - 3.5*radiusY < -1.0f) {
                b.y = -1.0f + 3.5*radiusY;
                b.vy *= -bounceFactor;
            }

            // Horizontal bounce
            if (b.x + radiusX > 1.0f) {
                b.x = 1.0f - radiusX;
                b.vx *= -bounceFactor;
            } else if (b.x - radiusX < -1.0f) {
                b.x = -1.0f + radiusX;
                b.vx *= -bounceFactor;
            }
        }
    }
}

// --- Draw all balls ---
void drawBalls() {
    float vertices[2 * (CIRCLE_SEGMENTS + 2)];
    for (auto &b : balls) {
        createCircleVertices(b.x, b.y, BALL_RADIUS, vertices);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        GLint posAttrib = glGetAttribLocation(program, "aPosition");
        glEnableVertexAttribArray(posAttrib);
        glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

        glUniform4f(colorLocation, b.r, b.g, b.b, 1.0f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGMENTS + 2);
    }
}

// --- Render loop ---
void loop() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!pauseSimulation) {
        updatePhysics(1.0f); // simple fixed timestep
    }
    drawBalls();
}

// --- Init WebGL ---
void init() {
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.alpha = EM_TRUE;
    attr.depth = EM_TRUE;
    attr.majorVersion = 2;
    ctx = emscripten_webgl_create_context("#canvas", &attr);
    emscripten_webgl_make_context_current(ctx);

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glUseProgram(program);

    glGenBuffers(1, &vertexBuffer);
    colorLocation = glGetUniformLocation(program, "uColor");

    resizeCanvas();
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE,
                                   [](int e, const EmscriptenUiEvent*, void*) -> EM_BOOL { resizeCanvas(); return EM_TRUE; });

    emscripten_set_click_callback("#canvas", nullptr, EM_TRUE, onMouseClick);
}

int main() {
    init();
    emscripten_set_main_loop(loop, 0, 1);
    return 0;
}