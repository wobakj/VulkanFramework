#ifndef MATERIAL_DATABASE_HPP
#define MATERIAL_DATABASE_HPP

#include "ren/database.hpp"
#include "ren/material.hpp"

#include "wrap/buffer.hpp"
#include "wrap/buffer_view.hpp"
#include "allocator_static.hpp"

#include <vector>

class Device;
class CommandBuffer;

class MaterialDatabase : public Database<material_t> {
 public:
  MaterialDatabase();
  MaterialDatabase(Transferrer& transferrer);
  MaterialDatabase(MaterialDatabase && dev);
  MaterialDatabase(MaterialDatabase const&) = delete;
  
  MaterialDatabase& operator=(MaterialDatabase const&) = delete;
  MaterialDatabase& operator=(MaterialDatabase&& dev);

  void store(std::string const& name, material_t&& resource) override;
  size_t index(std::string const& name) const;
 
  void swap(MaterialDatabase& dev);

  void updateCommand(CommandBuffer& buffer) const;

 private:
  std::map<std::string, size_t> m_indices;
  std::vector<BufferView> m_views;

  StaticAllocator m_allocator;
  Buffer m_buffer;
};

#endif