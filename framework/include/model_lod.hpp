#ifndef MODEL_LOD_HPP
#define MODEL_LOD_HPP

#include "buffer.hpp"
#include "buffer_view.hpp"
#include "memory.hpp"
#include "model_t.hpp"
#include "double_buffer.hpp"

#include "bvh.h"
#include "lod_stream.h"
#include <vulkan/vulkan.hpp>
#include <glm/gtc/type_precision.hpp>

#include <set>
#include <queue>
#include <atomic>

class Device;
class Camera;

class ModelLod {
 public:  
  ModelLod();
  ModelLod(Device& device, vklod::bvh const& bvh, std::string const& path, std::size_t num_nodes, std::size_t num_uploads);
  // ModelLod(Device& device, vklod::bvh const& bvh, lamure::ren::lod_stream&& stream, std::size_t num_nodes, std::size_t num_uploads);
  ModelLod(ModelLod && dev);
  ModelLod(ModelLod const&) = delete;
  ~ModelLod();

  ModelLod& operator=(ModelLod const&) = delete;
  ModelLod& operator=(ModelLod&& dev);

  void swap(ModelLod& dev);

  std::vector<std::size_t> const& cut() const;
  std::vector<std::size_t> const& activeBuffers() const;
  std::size_t numNodes() const;

  BufferView const& bufferView(std::size_t i = 0) const;
  vk::Buffer const& buffer() const;
  vk::PipelineVertexInputStateCreateInfo inputInfo() const;
  std::uint32_t numVertices() const;

  void update(Camera const& cam);
  void performCopiesCommand(vk::CommandBuffer const& command_buffer);
  void updateDrawCommands(vk::CommandBuffer const& command_buffer);
  std::vector<vk::DrawIndirectCommand> const& drawCommands() const;
  BufferView const& viewDrawCommands() const;
  void performCopies();

 private:
  void setFirstCut();
  void createStagingBuffers();
  void createDrawingBuffers();
  void nodeToSlotImmediate(std::size_t node, std::size_t buffer);
  
  void setCut(std::vector<std::size_t> const& cut);
  void nodeToSlot(std::size_t node, std::size_t buffer);
  void performUploads();
  void updateDrawCommands();

  bool nodeSplitable(std::size_t node);
  float nodeError(glm::fvec3 const& pos_view, std::size_t node);
  float collapseError(uint64_t idx_node);
  bool inCore(std::size_t idx_node);
  
  void printCut() const;
  void printSlots() const;

  model_t m_model;
  Device const* m_device;
  vk::VertexInputBindingDescription m_bind_info;
  std::vector<vk::VertexInputAttributeDescription> m_attrib_info;
  Buffer m_buffer;
  Buffer m_buffer_stage;
  std::vector<BufferView> m_buffer_views;
  std::vector<BufferView> m_buffer_views_stage;
  BufferView m_view_draw_commands;
  Memory m_memory;
  Memory m_memory_stage;
  std::size_t m_num_nodes; 
  std::size_t m_num_uploads;
  std::size_t m_num_slots; 
  vklod::bvh m_bvh;
  vk::DeviceSize m_size_node;
  std::vector<std::vector<float>> m_nodes;
  std::vector<std::size_t> m_cut;
  std::vector<std::size_t> m_slots;
  std::vector<std::size_t> m_active_slots;
  std::vector<std::pair<std::size_t, std::size_t>> m_node_uploads;
  std::vector<vk::DrawIndirectCommand> m_commands_draw;
  DoubleBuffer<std::vector<BufferView>> m_db_views_stage;
  uint8_t* m_ptr_mem_stage;
};

#endif