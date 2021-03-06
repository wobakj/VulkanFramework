#ifndef MODEL_LOD_HPP
#define MODEL_LOD_HPP

#include "wrap/buffer.hpp"
#include "wrap/buffer_view.hpp"
#include "wrap/memory.hpp"
#include "vertex_data.hpp"
#include "double_buffer.hpp"
#include "allocator_static.hpp"

#include "bvh.h"
#include "lod_stream.h"
#include <vulkan/vulkan.hpp>
#include <glm/gtc/type_precision.hpp>

#include <set>
#include <queue>
#include <atomic>

class Device;
class Camera;
class Transferrer;
class VertexInfo;

struct serialized_vertex {
  float v0_x_, v0_y_, v0_z_;   //vertex 0
  float n0_x_, n0_y_, n0_z_;   //normal 0
  float c0_x_, c0_y_;          //texcoord 0
};

class GeometryLod {
 public:  
  GeometryLod();
  GeometryLod(Transferrer& transferrer, std::string const& path, std::size_t cut_budget, std::size_t upload_budget);
  // GeometryLod(Device& device, vklod::bvh const& bvh, lamure::ren::lod_stream&& stream, std::size_t num_nodes, std::size_t num_uploads);
  GeometryLod(GeometryLod && dev);
  GeometryLod(GeometryLod const&) = delete;

  GeometryLod& operator=(GeometryLod const&) = delete;
  GeometryLod& operator=(GeometryLod&& dev);

  void swap(GeometryLod& dev);

  std::vector<std::size_t> const& cut() const;
  std::vector<std::size_t> const& activeBuffers() const;
  std::size_t numNodes() const;

  BufferView const& bufferView(std::size_t i = 0) const;
  Buffer const& buffer() const;
  std::uint32_t numVertices() const;
  std::size_t numUploads() const;
  std::size_t sizeNode() const;

  VertexInfo vertexInfo() const;

  void update(Camera const& cam);
  void update(glm::fmat4 const& view, glm::fmat4 const& projection);
  void performCopiesCommand(vk::CommandBuffer const& command_buffer);
  void updateDrawCommands(vk::CommandBuffer const& command_buffer);
  std::vector<vk::DrawIndirectCommand> const& drawCommands() const;
  BufferView const& viewDrawCommands() const;
  BufferView const& viewNodeLevels() const;
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
  bool nodeCollapsible(std::size_t node);
  float nodeError(glm::fvec3 const& pos_view, std::size_t node);
  float collapseError(uint64_t idx_node);
  bool inCore(std::size_t idx_node);
  
  void printCut() const;
  void printSlots() const;
  void updateResourcePointers();

  StaticAllocator m_allocator_draw;
  StaticAllocator m_allocator_stage;

  vertex_data m_model;
  Device const* m_device;
  Transferrer const* m_transferrer;
  Buffer m_buffer;
  Buffer m_buffer_stage;
  std::vector<BufferView> m_buffer_views;
  std::vector<BufferView> m_buffer_views_stage;
  BufferView m_view_draw_commands;
  BufferView m_view_levels;
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
  DoubleBuffer<std::vector<BufferRegion>> m_db_views_stage;
  uint8_t* m_ptr_mem_stage;
};

#endif