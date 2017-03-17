#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "node/node_model.hpp"
#include "ren/application_instance.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Transferrer;

class CommandBuffer;

class Renderer {
 public:  
  Renderer();
  Renderer(ApplicationInstance& instance);
  Renderer(Renderer && dev);
  Renderer(Renderer const&) = delete;

  Renderer& operator=(Renderer const&) = delete;
  Renderer& operator=(Renderer&& dev);

  void swap(Renderer& dev);
  void draw(CommandBuffer& buffer, std::vector<ModelNode const*> const& nodes);

 private:
  ApplicationInstance* m_instance;
};

#endif