#pragma once
#include <functional>

#include "view/opengl.h"

struct WindowDef {
    // width / height is size of render area (doesn't include window decorations)
    int width = 800;
    int height = 600;
    const char* title = "";
    bool fullscreen = false;
    bool vsync = true;
    bool resizeable = true;
};

GLFWwindow* CreateWindow(WindowDef wd);
void RunEventLoop(GLFWwindow* window, const std::function<void()>& callback);
