#ifndef MATERIAL_DATABASE_HPP
#define MATERIAL_DATABASE_HPP

#include "ren/database.hpp"
#include "ren/material.hpp"

#include "wrap/memory.hpp"

#include <vulkan/vulkan.hpp>
// use floats and med precision operations
#include <glm/gtc/type_precision.hpp>

#include <tiny_obj_loader.h>

#include <map>

class Device;
class Image;

class MaterialDatabase : public Database<material_t> {
 public:
  MaterialDatabase();
  MaterialDatabase(Transferrer& transferrer);
  MaterialDatabase(MaterialDatabase && dev);
  MaterialDatabase(MaterialDatabase const&) = delete;
  
  MaterialDatabase& operator=(MaterialDatabase const&) = delete;
  MaterialDatabase& operator=(MaterialDatabase&& dev);

  std::map<std::string, size_t> m_indices;
};

#endif