#ifndef MODEL_HPP
#define MODEL_HPP

#include "buffer.hpp"
#include "model_t.hpp"

#include <vulkan/vulkan.hpp>

class Device;

class Model {
 public:  
  Model();
  Model(Device const& device, model_t const& model);
  Model(Model && dev);
  Model(Model const&) = delete;

  Model& operator=(Model const&) = delete;
  Model& operator=(Model&& dev);

  void swap(Model& dev);

  vk::Buffer const& bufferVertex() const;
  vk::Buffer const& bufferIndex() const;
  
  vk::PipelineVertexInputStateCreateInfo inputInfo() const;
  std::uint32_t numIndices() const;

 private:
  model_t m_model;
  Device const* m_device;
  vk::VertexInputBindingDescription m_bind_info;
  std::vector<vk::VertexInputAttributeDescription> m_attrib_info;
  Buffer m_buffer_vertex;
  Buffer m_buffer_index;
};

#endif