#ifndef MODEL_HPP
#define MODEL_HPP

#include "geometry.hpp"
#include "ren/material.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Transferrer;

class Model {
 public:  
  Model();
  Model(Transferrer& transferrer, vertex_data const& model);
  Model(Model && dev);
  Model(Model const&) = delete;

  Model& operator=(Model const&) = delete;
  Model& operator=(Model&& dev);

  void swap(Model& dev);

 private:
  std::vector<Geometry> m_geometries;
  std::vector<material_t> m_materials;
  vk::PrimitiveTopology m_topology;
};

#endif