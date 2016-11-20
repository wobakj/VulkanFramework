#ifndef MODEL_LOD_HPP
#define MODEL_LOD_HPP

#include "buffer.hpp"
#include "memory.hpp"
#include "model_t.hpp"

#include "bvh.h"
#include "lod_stream.h"

#include <vulkan/vulkan.hpp>

class Device;

class ModelLod {
 public:  
  ModelLod();
  ModelLod(Device& device, vklod::bvh const& bvh, std::string const& path, std::size_t num_nodes, std::size_t num_uploads);
  // ModelLod(Device& device, vklod::bvh const& bvh, lamure::ren::lod_stream&& stream, std::size_t num_nodes, std::size_t num_uploads);
  ModelLod(ModelLod && dev);
  ModelLod(ModelLod const&) = delete;

  ModelLod& operator=(ModelLod const&) = delete;
  ModelLod& operator=(ModelLod&& dev);

  void swap(ModelLod& dev);

  vk::Buffer const& buffer() const;
  vk::PipelineVertexInputStateCreateInfo inputInfo() const;
  std::uint32_t numVertices() const;

 private:
  model_t m_model;
  Device const* m_device;
  vk::VertexInputBindingDescription m_bind_info;
  std::vector<vk::VertexInputAttributeDescription> m_attrib_info;
  std::vector<Buffer> m_buffers;
  std::vector<Buffer> m_buffers_stage;
  Memory m_memory;
  Memory m_memory_stage;
  std::size_t m_num_nodes; 
  std::size_t m_num_uploads;

  vklod::bvh m_bvh;
  std::vector<std::vector<float>> m_nodes;
};

#endif