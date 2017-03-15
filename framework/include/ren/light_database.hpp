#ifndef LIGHT_DATABASE_HPP
#define LIGHT_DATABASE_HPP

#include "ren/database.hpp"
#include "ren/light.hpp"

#include "wrap/buffer.hpp"
#include "allocator_static.hpp"

#include <vector>

class Device;

class LightDatabase : public Database<light_t> {
 public:
  LightDatabase();
  LightDatabase(Device const& dev);
  LightDatabase(LightDatabase && rhs);
  LightDatabase(LightDatabase const&) = delete;
  
  LightDatabase& operator=(LightDatabase const&) = delete;
  LightDatabase& operator=(LightDatabase&& rhs);

  void store(std::string const& name, light_t&& mat) override;
  size_t index(std::string const& name) const;
 
  void swap(LightDatabase& rhs);
  light_t const& get(std::string const& name) override;
  void set(std::string const& name, light_t const& mat);

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