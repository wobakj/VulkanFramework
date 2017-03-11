#include "ren/model_loader.hpp"

#include "ren/material_database.hpp"
#include "ren/geometry_database.hpp"
#include "ren/model_database.hpp"
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

ModelLoader::ModelLoader(Transferrer& transfer, MaterialDatabase& mat, GeometryDatabase& geo, ModelDatabase& model)
 :m_transferrer{&transfer}
 ,m_database_geo(&geo)
 ,m_database_mat(&mat)
 ,m_database_model(&model)
{}

ModelLoader& ModelLoader::operator=(ModelLoader&& rhs) {
  swap(rhs);
  return *this;
}

void ModelLoader::swap(ModelLoader& rhs) {
  std::swap(m_database_geo, rhs.m_database_geo);
  std::swap(m_database_mat, rhs.m_database_mat);
  std::swap(m_database_model, rhs.m_database_model);
}

Model ModelLoader::load(std::string const& filename, vertex_data::attrib_flag_t import_attribs) const {
  // load gpu data reprsentations
  auto result = geometry_loader::objs(filename, import_attribs);
  auto& vert_datas = result.first;
  auto& mat_datas = result.second;
  std::vector<std::string> keys_geo{};
  std::vector<std::string> keys_mat{};
  for (size_t i = 0; i < vert_datas.size(); ++i) {
    std::string key_geo{filename + '|' + std::to_string(i)};
    m_database_geo->store(key_geo, Geometry{*m_transferrer, vert_datas[i]});
    keys_mat.emplace_back(key_geo);

    std::string key_mat{filename + '|' + std::to_string(i)};
    m_database_mat->store(key_mat, std::move(mat_datas[i]));
    keys_mat.emplace_back(key_mat);
  }

  return Model{keys_geo, keys_mat};
}

void ModelLoader::store(std::string const& filename, vertex_data::attrib_flag_t import_attribs) const {
  m_database_model->store(filename, load(filename, import_attribs));
}