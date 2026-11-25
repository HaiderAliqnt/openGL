#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

const char* kVertexShader = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

uniform float uAngle;   // radians, counter-clockwise
uniform float uAspect;  // framebuffer width / height

out vec3 vColor;

void main()
{
    float c = cos(uAngle);
    float s = sin(uAngle);
    vec2 p = mat2(c, s, -s, c) * aPos;  // GLSL matrices are column-major

    // Shrink the longer axis so the triangle isn't stretched.
    p *= uAspect > 1.0 ? vec2(1.0 / uAspect, 1.0) : vec2(1.0, uAspect);

    gl_Position = vec4(p, 0.0, 1.0);
    vColor = aColor;
}
)";

const char* kFragmentShader = R"(
#version 330 core
in vec3 vColor;
out vec4 fragColor;

void main()
{
    fragColor = vec4(vColor, 1.0);
}
)";

// Returns 0 and prints the log if compilation fails.
GLuint compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error:\n" << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Returns 0 and prints the log if compiling or linking fails.
GLuint createProgram()
{
    GLuint vertex = compileShader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);

    GLuint program = 0;
    if (vertex && fragment) {
        program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);

        GLint ok = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            std::cerr << "Program link error:\n" << log << std::endl;
            glDeleteProgram(program);
            program = 0;
        }
    }

    // Deleting 0 is a no-op; attached shaders are freed along with the program.
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

} // namespace

int main()
{
    glfwSetErrorCallback([](int code, const char* description) {
        std::cerr << "GLFW error " << code << ": " << description << std::endl;
    });

    if (!glfwInit())
        return EXIT_FAILURE;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "openGL", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);

    // GLAD can only load function pointers once a context is current.
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }
    std::cout << "OpenGL " << glGetString(GL_VERSION) << " on "
              << glGetString(GL_RENDERER) << std::endl;

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int width, int height) {
        glViewport(0, 0, width, height);
    });
    glfwSwapInterval(1);

    GLuint program = createProgram();
    if (!program) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }
    const GLint angleLocation = glGetUniformLocation(program, "uAngle");
    const GLint aspectLocation = glGetUniformLocation(program, "uAspect");

    // Equilateral triangle centred on the origin: x, y, r, g, b
    const float vertices[] = {
         0.0f,     0.6f,   1.0f, 0.0f, 0.0f,
        -0.5196f, -0.3f,   0.0f, 1.0f, 0.0f,
         0.5196f, -0.3f,   0.0f, 0.0f, 1.0f,
    };

    GLuint vao = 0;
    GLuint vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    const GLsizei stride = 5 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    const double radiansPerSecond = 1.5;
    const double fullTurn = 6.283185307179586;

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);

        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);

        if (width > 0 && height > 0) {  // both are 0 while minimised
            // Driven by elapsed time, so the speed doesn't depend on frame rate.
            const double angle = std::fmod(glfwGetTime() * radiansPerSecond, fullTurn);

            glUseProgram(program);
            glUniform1f(angleLocation, static_cast<float>(angle));
            glUniform1f(aspectLocation, static_cast<float>(width) / static_cast<float>(height));
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(program);

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
