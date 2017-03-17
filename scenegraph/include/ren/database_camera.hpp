#ifndef DATABASE_CAMERA_HPP
#define DATABASE_CAMERA_HPP

#include "ren/database.hpp"
#include "camera.hpp"
// #include "gpu_camera.hpp"

#include "wrap/buffer.hpp"
#include "wrap/buffer_view.hpp"
#include "allocator_static.hpp"

#include <vector>

class Device;

class CameraDatabase : public Database<Camera> {
 public:
  CameraDatabase();
  CameraDatabase(Device const& dev);
  CameraDatabase(CameraDatabase && rhs);
  CameraDatabase(CameraDatabase const&) = delete;
  
  CameraDatabase& operator=(CameraDatabase const&) = delete;
  CameraDatabase& operator=(CameraDatabase&& rhs);

  void store(std::string const& name, Camera&& mat) override;
  size_t index(std::string const& name) const;
 
  void swap(CameraDatabase& rhs);
  // Camera const& get(std::string const& name) override;
  void set(std::string const& name, Camera const& mat);
  void set(std::string const& name, Camera&& mat);

  void updateCommand(CommandBuffer const& buffer) const;
  
  Buffer const& buffer() const {
    return m_buffer;
  }
  
 private:
  std::map<std::string, size_t> m_indices;
  mutable std::vector<size_t> m_dirties;
  uint8_t* m_ptr_mem_stage;

  StaticAllocator m_allocator;
  StaticAllocator m_allocator_stage;
  Buffer m_buffer;
  Buffer m_buffer_stage;
};

#endif