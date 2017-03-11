#include "ren/model.hpp"

// #include "wrap/device.hpp"
// #include "transferrer.hpp"

#include <iostream>

Model::Model()
{}

Model::Model(Model && rhs)
 :Model{}
{
  swap(rhs);
}

Model::Model(std::vector<std::string> const& geo, std::vector<std::string> const& mat, vk::PrimitiveTopology const& topo)
 :m_geometries(geo)
 ,m_materials(mat)
 ,m_topology{topo}
{}

Model& Model::operator=(Model&& rhs) {
  swap(rhs);
  return *this;
}

void Model::swap(Model& rhs) {
  std::swap(m_geometries, rhs.m_geometries);
  std::swap(m_materials, rhs.m_materials);
  std::swap(m_topology, rhs.m_topology);
}