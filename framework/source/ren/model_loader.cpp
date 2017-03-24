#include "ren/model_loader.hpp"

#include "ren/material_database.hpp"
#include "ren/geometry_database.hpp"
#include "ren/model_database.hpp"
#include "ren/application_instance.hpp"
#include "transferrer.hpp"
#include "geometry_loader.hpp"

#include <iostream>

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
  }
  if (mat_datas.empty()) {
    std::cerr << "Model " << filename << " has no materials, using default." << std::endl;
  }
  return Model{keys_geo, keys_mat};
}

void ModelLoader::store(std::string const& filename, vertex_data::attrib_flag_t import_attribs) const {
  m_instance->dbModel().store(filename, load(filename, import_attribs));
}