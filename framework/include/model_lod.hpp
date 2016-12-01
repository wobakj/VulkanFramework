#ifndef MODEL_LOD_HPP
#define MODEL_LOD_HPP

#include "buffer.hpp"
#include "memory.hpp"
#include "model_t.hpp"

#include "bvh.h"
#include "lod_stream.h"
#include <vulkan/vulkan.hpp>
#include <glm/gtc/type_precision.hpp>

#include <set>

class Device;

class ModelLod {
 public:  
  ModelLod();
  ModelLod(Device& device, vklod::bvh const& bvh, std::string const& path, std::size_t num_nodes, std::size_t num_uploads);
  // ModelLod(Device& device, vklod::bvh const& bvh, lamure::ren::lod_stream&& stream, std::size_t num_nodes, std::size_t num_uploads);
  ModelLod(ModelLod && dev);
  ModelLod(ModelLod const&) = delete;

  void createStagingBuffers();
  void createDrawingBuffers();
  void setFirstCut();

  ModelLod& operator=(ModelLod const&) = delete;
  ModelLod& operator=(ModelLod&& dev);

  void swap(ModelLod& dev);

  std::vector<std::size_t> const& cut() const;
  std::vector<std::size_t> const& activeBuffers() const;
  std::vector<std::size_t> const& activeBuffers2() const;

  vk::Buffer const& buffer(std::size_t i = 0) const;
  vk::PipelineVertexInputStateCreateInfo inputInfo() const;
  std::uint32_t numVertices() const;

  void update(glm::fvec3 const& pod_view);

 private:
  void setCut(std::vector<std::size_t> const& cut);
  void nodeToBuffer(std::size_t node, std::size_t buffer);
  float nodeError(glm::fvec3 const& pos_view, std::size_t node);
  bool nodeSplitable(std::size_t node);

  model_t m_model;
  Device const* m_device;
  vk::VertexInputBindingDescription m_bind_info;
  std::vector<vk::VertexInputAttributeDescription> m_attrib_info;
  std::vector<Buffer> m_buffers;
  std::vector<Buffer> m_buffers_stage;
  Memory m_memory;
  Memory m_memory_stage;
  std::size_t m_num_nodes; 
  std::size_t m_num_slots; 
  std::size_t m_num_uploads;
  vklod::bvh m_bvh;
  vk::DeviceSize m_size_node;
  std::vector<std::vector<float>> m_nodes;
  std::vector<std::size_t> m_cut;
  std::vector<std::size_t> m_slots;
  std::set<std::size_t> m_slots_keep;
  std::vector<std::size_t> m_active_buffers;
  std::vector<std::size_t> m_active_buffers2;
};

#endif