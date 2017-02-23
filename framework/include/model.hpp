#ifndef MODEL_HPP
#define MODEL_HPP

#include "buffer.hpp"
#include "memory.hpp"
#include "model_t.hpp"

#include <vulkan/vulkan.hpp>

class Device;

class Model {
 public:  
  Model();
  Model(Device& device, model_t const& model);
  Model(Model && dev);
  Model(Model const&) = delete;

  Model& operator=(Model const&) = delete;
  Model& operator=(Model&& dev);

  void swap(Model& dev);

  vk::Buffer const& buffer() const;
  vk::DeviceSize indexOffset() const;
  vk::PipelineVertexInputStateCreateInfo inputInfo() const;
  std::vector<vk::VertexInputBindingDescription> const& bindInfos() const;
  std::vector<vk::VertexInputAttributeDescription> const& attributeInfos() const;
  std::uint32_t numIndices() const;
  std::uint32_t numVertices() const;

 private:
  model_t m_model;
  Device const* m_device;
  std::vector<vk::VertexInputBindingDescription> m_bind_info;
  std::vector<vk::VertexInputAttributeDescription> m_attrib_info;
  Buffer m_buffer;
  Memory m_memory;
};

#endif