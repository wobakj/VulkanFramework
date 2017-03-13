#include "ren/renderer.hpp"

#include "wrap/command_buffer.hpp"
#include "ren/material_database.hpp"
#include "ren/geometry_database.hpp"
#include "ren/model_database.hpp"
#include "ren/transform_database.hpp"
#include "transferrer.hpp"
#include "geometry_loader.hpp"

#include <iostream>

Renderer::Renderer()
{}

Renderer::Renderer(Renderer && rhs)
 :Renderer{}
{
  swap(rhs);
}

Renderer::Renderer(MaterialDatabase& mat, GeometryDatabase& geo, ModelDatabase& model, TransformDatabase& trans)
 :m_database_geo(&geo)
 ,m_database_mat(&mat)
 ,m_database_model(&model)
 ,m_database_transform{&trans}
{}

Renderer& Renderer::operator=(Renderer&& rhs) {
  swap(rhs);
  return *this;
}

void Renderer::swap(Renderer& rhs) {
  std::swap(m_database_geo, rhs.m_database_geo);
  std::swap(m_database_mat, rhs.m_database_mat);
  std::swap(m_database_model, rhs.m_database_model);
}

void Renderer::draw(CommandBuffer& buffer, std::vector<Node const*> const& nodes) {
  // store transforms per geometry per material
  std::map<std::string, std::map<std::string, std::vector<size_t>>> material_geometries;
  // collect geometries for materials
  for (auto const& node_ptr : nodes) {
    auto const& model = m_database_model->get(node_ptr->m_model);
    auto const& index_transform = m_database_transform->index(node_ptr->m_transform);

    for(size_t i = 0; i < model.m_geometries.size(); ++i) {
      auto const& name_geo = model.m_geometries[i];
      auto const& name_mat = model.m_materials[i];
      // auto ptr_geo = &m_database_geo->get(name_geo);
      // check if material exists
      auto iter_material = material_geometries.find(name_mat);
      if (iter_material == material_geometries.end()) {
        material_geometries.emplace(name_mat, std::map<std::string, std::vector<size_t>>{});
        material_geometries.at(name_mat).emplace(name_geo, std::vector<size_t>(1, index_transform));
      }
      else {
        // check if geometry exists
        auto iter_geo = iter_material->second.find(name_geo);
        if (iter_geo == iter_material->second.end()) {
          iter_material->second.emplace(name_geo, std::vector<size_t>(1, index_transform));
        }
        else {
          // store model for this geometry
          iter_geo->second.emplace_back(index_transform);
        }
      }
    }
  }
  for (auto const& material_entry : material_geometries) {
    uint32_t const& material_idx = uint32_t(m_database_mat->index(material_entry.first));
    buffer.pushConstants(vk::ShaderStageFlagBits::eFragment, 0, material_idx);
    for (auto const& geometry_entry : material_entry.second) {
      auto const& geometry = m_database_geo->get(geometry_entry.first);
      buffer.bindGeometry(geometry);
      for (auto const& transform_entry : geometry_entry.second) {
        uint32_t transform_idx = uint32_t(transform_entry);
        buffer.pushConstants(vk::ShaderStageFlagBits::eFragment, 0, transform_idx);

      }
    }
  }
}