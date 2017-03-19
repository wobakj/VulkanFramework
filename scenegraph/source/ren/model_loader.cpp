#include "ren/model_loader.hpp"

#include "ren/database_material.hpp"
#include "ren/database_geometry.hpp"
#include "ren/database_model.hpp"
#include "ren/application_instance.hpp"
#include "transferrer.hpp"
#include "geometry_loader.hpp"

#include <iostream>
#include <limits>

ModelLoader::ModelLoader()
{}

ModelLoader::ModelLoader(ModelLoader && rhs)
 :ModelLoader{}
{
  swap(rhs);
}

ModelLoader::ModelLoader(ApplicationInstance& instance)
 :m_instance{&instance}
{}

ModelLoader& ModelLoader::operator=(ModelLoader&& rhs) {
  swap(rhs);
  return *this;
}

void ModelLoader::swap(ModelLoader& rhs) {
  std::swap(m_instance, rhs.m_instance);
}

Model ModelLoader::load(std::string const& filename, vertex_data::attrib_flag_t import_attribs) const {
  // load gpu data representations
  auto result = geometry_loader::objs(filename, import_attribs);
  auto& vert_datas = result.first;
  auto& mat_datas = result.second;
  std::vector<std::string> keys_geo{};
  std::vector<std::string> keys_mat{};
  glm::vec3 box_min(std::numeric_limits<float>::max()); 
  glm::vec3 box_max(std::numeric_limits<float>::lowest());
  for (size_t i = 0; i < vert_datas.size(); ++i) {
    // store geometry
    std::string key_geo{filename + '|' + std::to_string(i)};
    m_instance->dbGeometry().store(key_geo, Geometry{m_instance->transferrer(), vert_datas[i]});
    keys_geo.emplace_back(key_geo);
    
    std::string key_mat{filename + '|' + std::to_string(i)};
    if (mat_datas.empty()) {
      key_mat = "default";
    }
    else {
      // load new textures
      for (auto const& tex_pair : mat_datas[i].textures) {
        if (!m_instance->dbTexture().contains(tex_pair.second)) {
          m_instance->dbTexture().store(tex_pair.second);
        }
      }
      // store material
      if (!m_instance->dbMaterial().contains(key_mat)) {
        m_instance->dbMaterial().store(key_mat, std::move(mat_datas[i]));
      }
    }
    keys_mat.emplace_back(key_mat);
    // adjust bbox
    for (unsigned int j = 0; j < vert_datas[i].data.size(); j += vert_datas[i].vertex_bytes){
        if (vert_datas[i].data[j] < box_min.x) box_min.x = vert_datas[i].data[j];
        if (vert_datas[i].data[j+1] < box_min.y) box_min.y = vert_datas[i].data[j+1];
        if (vert_datas[i].data[j+2] < box_min.z) box_min.z = vert_datas[i].data[j+2];

        if (vert_datas[i].data[j] > box_max.x) box_max.x = vert_datas[i].data[j];
        if (vert_datas[i].data[j+1] > box_max.y) box_max.y = vert_datas[i].data[j+1];
        if (vert_datas[i].data[j+2] > box_max.z) box_max.z = vert_datas[i].data[j+2];
    }
  }
  if (mat_datas.empty()) {
    std::cerr << "Model " << filename << " has no materials, using default." << std::endl;
  }
  return Model{keys_geo, keys_mat, box_min, box_max};
}

void ModelLoader::store(std::string const& filename, vertex_data::attrib_flag_t import_attribs) const {
  m_instance->dbModel().store(filename, load(filename, import_attribs));
}