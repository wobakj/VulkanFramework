#ifndef SURFACE_HPP
#define SURFACE_HPP

#include "wrap/wrapper.hpp"

#include <vulkan/vulkan.hpp>

class Instance;
class GLFWwindow;

using WrapperSurface = Wrapper<vk::SurfaceKHR, GLFWwindow*>;
class Surface : public WrapperSurface {
 public:
  Surface();
  Surface(Instance const& inst, GLFWwindow& win);
  Surface(Surface && dev);
  Surface(Surface const&) = delete;
  ~Surface();
  
  Surface& operator=(Surface const&) = delete;
  Surface& operator=(Surface&& dev);

  void swap(Surface& dev);
  // GLFWwindow const& window() const;
  GLFWwindow& window() const;

 private:
  void destroy() override;

  Instance const* m_instance;
};

#endif