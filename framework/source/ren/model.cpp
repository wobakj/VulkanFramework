#include "ren/model.hpp"

#include "wrap/device.hpp"
#include "transferrer.hpp"

#include <iostream>

Model::Model()
 // :m_model{}
{}

Model::Model(Model && rhs)
 :Model{}
{
  swap(rhs);
}

Model::Model(Transferrer& transferrer, vertex_data const& model)
 // :m_model{model}
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