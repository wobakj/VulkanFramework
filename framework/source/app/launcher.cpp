#include "app/launcher.hpp"

//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "app/application.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
// SwapChain m_swap_chain;

// helper functions
static std::string resourcePath(std::string const& path_exe);
void glfw_error(int error, const char* description);

Launcher::Launcher(std::string const& path_exe, bool debug) 
 :m_last_second_time{0.0}
 ,m_frames_per_second{0u}
 ,m_resource_path{resourcePath(path_exe)}
 ,m_application{}
 ,m_instance{}
 ,m_device{}
 ,m_validation_layers{{"VK_LAYER_LUNARG_standard_validation"}}
{
  
  auto extensions = vk::enumerateInstanceExtensionProperties(nullptr);

  std::cout << "available extensions:" << std::endl;

  for (const auto& extension : extensions) {
    std::cout << "\t" << extension.extensionName << std::endl;
  }

  bool validate = true;
  #ifdef NDEBUG
    validate = debug;
  #endif
  m_instance.create(validate);

  m_device = m_instance.createLogicalDevice({});
}

static std::string resourcePath(std::string const& path_exe) {
  std::string resource_path{};
  resource_path = path_exe.substr(0, path_exe.find_last_of("/\\"));
  resource_path += "/resources/";
  return resource_path;
}
 
void Launcher::mainLoop() {
  m_device->waitIdle();

  // rendering loop
  while (!m_application->shouldClose()) {
    // draw geometry
    m_application->frame();
  }

  quit(EXIT_SUCCESS);
}

///////////////////////////// update functions ////////////////////////////////
// update viewport and field of view
void Launcher::resize(int width, int height) {
  if (width > 0 && height > 0) {
    // draw remaining recorded frames
    m_application->emptyDrawQueue();
    // rebuild resources
    m_application->resize(width, height);
  }
}
///////////////////////////// misc functions ////////////////////////////////
void Launcher::quit(int status) {
  // wait until all resources are accessible
  m_device->waitIdle();
  // free opengl resources
  m_application.reset();
}