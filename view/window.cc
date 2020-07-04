#include "view/window.h"

static void error_callback(int error, const char* message) {
    fprintf(stderr, "GLFW error %d: %s\n", error, message);
    exit(0);
}

GLFWwindow* CreateWindow(WindowDef wd) {
    if (!glfwInit()) exit(1);

    glfwSetErrorCallback(error_callback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, wd.resizeable);

    // TODO fullscreen
    auto window = glfwCreateWindow(wd.width, wd.height, wd.title, nullptr, nullptr);
    if (!window) exit(1);

    glfwMakeContextCurrent(window);
#ifdef __linux__
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) exit(1);
#endif
    glfwSwapInterval(wd.vsync ? 1 : 0);

    return window;
}

void RunEventLoop(GLFWwindow* window, const std::function<void()>& callback) {
    // Blank screen workaround in OSX Mojave
    glfwPollEvents();
    int x, y;
    glfwGetWindowPos(window, &x, &y);
    glfwSetWindowPos(window, x + 1, y);

    while (!glfwWindowShouldClose(window)) {
        callback();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
}
