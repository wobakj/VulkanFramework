#include "wrap/surface.hpp"

#include "wrap/instance.hpp"

//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

Surface::Surface()
 :WrapperSurface{}
 ,m_instance{nullptr}
{}

Surface::Surface(Surface && Surface)
 :WrapperSurface{}
{
  swap(Surface);
}

Surface::Surface(Instance const& inst, GLFWwindow& win)
 :Surface{}
{
  m_instance = &inst;
  m_info = &win;
  if (glfwCreateWindowSurface(m_instance->get(), m_info, nullptr, reinterpret_cast<VkSurfaceKHR*>(&m_object)) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
}

Surface::~Surface() {
  cleanup();
}

void Surface::destroy() {
  (*m_instance)->destroySurfaceKHR(get());
}

Surface& Surface::operator=(Surface&& Surface) {
  swap(Surface);
  return *this;
}

void Surface::swap(Surface& rhs) {
  WrapperSurface::swap(rhs);
  std::swap(m_instance, rhs.m_instance);
}

// GLFWwindow const& Surface::window() const {
//   return *info();
// }

GLFWwindow& Surface::window() const {
  return *info();
}
