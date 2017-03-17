#ifndef TRANSFORM_DATABASE_HPP
#define TRANSFORM_DATABASE_HPP

#include "ren/database.hpp"

#include "wrap/buffer.hpp"
#include "wrap/buffer_view.hpp"
#include "allocator_static.hpp"

// use floats and med precision operations
#include <glm/gtc/type_precision.hpp>

#include <vector>

class Device;

class TransformDatabase : public Database<glm::fmat4> {
 public:
  TransformDatabase();
  TransformDatabase(Device const& dev);
  TransformDatabase(TransformDatabase && rhs);
  TransformDatabase(TransformDatabase const&) = delete;
  
  TransformDatabase& operator=(TransformDatabase const&) = delete;
  TransformDatabase& operator=(TransformDatabase&& rhs);

  void store(std::string const& name, glm::fmat4&& mat) override;
  size_t index(std::string const& name) const;
 
  void swap(TransformDatabase& rhs);
  glm::fmat4 const& get(std::string const& name) override;
  void set(std::string const& name, glm::fmat4 const& mat);

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