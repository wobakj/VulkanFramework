#include "app/launcher_win.hpp"

//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "app/application.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
// SwapChain m_swap_chain;

// helper functions
std::string resourcePath(std::vector<std::string> const& args);
void glfw_error(int error, const char* description);

LauncherWin::LauncherWin(std::vector<std::string> const& args, cmdline::parser const& cmd_parse) 
 :m_camera_fov{glm::radians(60.0f)}
 ,m_window_width{1900}
 ,m_window_height{1070}
 ,m_window{nullptr}
 ,m_last_second_time{0.0}
 ,m_frames_per_second{0u}
 ,m_resource_path{resourcePath(args)}
 ,m_application{}
 ,m_instance{}
 ,m_device{}
 // ,m_swap_chain{}
 // ,m_surface{m_instance, vkDestroySurfaceKHR}
 ,m_validation_layers{{"VK_LAYER_LUNARG_standard_validation"}}
{
  glfwSetErrorCallback(glfw_error);

  if (!glfwInit()) {
    std::exit(EXIT_FAILURE);
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  // create m_window, if unsuccessfull, quit
  m_window = glfwCreateWindow(m_window_width, m_window_height, "Vulkan Framework", NULL, NULL);
  if (!m_window) {
    glfwTerminate();
    std::exit(EXIT_FAILURE);
  }

  auto extensions = vk::enumerateInstanceExtensionProperties(nullptr);

  std::cout << "available extensions:" << std::endl;

  for (const auto& extension : extensions) {
    std::cout << "\t" << extension.extensionName << std::endl;
  }

  bool validate = true;
  #ifdef NDEBUG
    if (!cmd_parse.exist("debug")) {
      validate = false;
    }
  #endif
  m_instance.create(validate);
  m_surface = Surface{m_instance, *m_window};
  // createSurface();
  // capure mouse
  glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  std::vector<const char*> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };
  m_device = m_instance.createLogicalDevice(vk::SurfaceKHR{m_surface}, deviceExtensions);

  // // set user pointer to access this instance statically
  glfwSetWindowUserPointer(m_window, this);
  // register resizing function
  auto resize_func = [](GLFWwindow* w, int a, int b) {
        static_cast<LauncherWin*>(glfwGetWindowUserPointer(w))->resize(w, a, b);
  };
  glfwSetFramebufferSizeCallback(m_window, resize_func);

  // // register key input function
  auto key_func = [](GLFWwindow* w, int a, int b, int c, int d) {
        static_cast<LauncherWin*>(glfwGetWindowUserPointer(w))->key_callback(w, a, b, c, d);
  };
  glfwSetKeyCallback(m_window, key_func);
}

std::string resourcePath(std::vector<std::string> const& args) {
  std::string resource_path{};
  std::string exe_path{args[0]};
  resource_path = exe_path.substr(0, exe_path.find_last_of("/\\"));
  resource_path += "/resources/";
  return resource_path;
}
 
void LauncherWin::mainLoop() {
  // resize(m_window, m_window_width, m_window_height);
  m_device->waitIdle();

  // rendering loop
  while (!glfwWindowShouldClose(m_window)) {
    // query input
    glfwPollEvents();
    // draw geometry
    m_application->frame();
    // display fps
    show_fps();
  }

  quit(EXIT_SUCCESS);
}

///////////////////////////// update functions ////////////////////////////////
// update viewport and field of view
void LauncherWin::resize(GLFWwindow* m_window, int width, int height) {
  if (width > 0 && height > 0) {
    // draw remaining recorded frames
    m_application->emptyDrawQueue();
    // rebuild resources
    m_application->resize(width, height);
  }
}
///////////////////////////// misc functions ////////////////////////////////
// handle key input
void LauncherWin::key_callback(GLFWwindow* m_window, int key, int scancode, int action, int mods) {
  if ((key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(m_window, 1);
  }
  else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
    m_application->updateShaderPrograms();
  }
  m_application->keyCallback(key, scancode, action, mods);
}

//handle mouse movement input
// void LauncherWin::mouse_callback(GLFWwindow* window, double pos_x, double pos_y) {
//   m_application->mouseCallback(pos_x, pos_y);
//   // reset cursor pos to receive position delta next frame
//   glfwSetCursorPos(m_window, 0.0, 0.0);
// }

// calculate fps and show in m_window title
void LauncherWin::show_fps() {
  ++m_frames_per_second;
  double current_time = glfwGetTime();
  if (current_time - m_last_second_time >= 1.0) {
    std::string title{"Vulkan Framework - "};
    title += std::to_string(m_frames_per_second) + " fps";

    glfwSetWindowTitle(m_window, title.c_str());
    m_frames_per_second = 0;
    m_last_second_time = current_time;
  }
}

void LauncherWin::quit(int status) {
  // wait until all resources are accessible
  m_device->waitIdle();
  // free opengl resources
  delete m_application;
  // free glfw resources
  glfwDestroyWindow(m_window);
  glfwTerminate();

  std::exit(status);
}

void glfw_error(int error, const char* description) {
  std::cerr << "GLFW Error " << error << " : "<< description << std::endl;
}
