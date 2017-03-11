#ifndef MODEL_LOADER_HPP
#define MODEL_LOADER_HPP

#include "wrap/vertex_data.hpp"

class Transferrer;
class GeometryDatabase;
class MaterialDatabase;
class ModelDatabase;
class Model;

class ModelLoader {
 public:  
  ModelLoader();
  ModelLoader(Transferrer& transfer, MaterialDatabase& mat, GeometryDatabase& geo, ModelDatabase& model);
  ModelLoader(ModelLoader && dev);
  ModelLoader(ModelLoader const&) = delete;

  ModelLoader& operator=(ModelLoader const&) = delete;
  ModelLoader& operator=(ModelLoader&& dev);

  void swap(ModelLoader& dev);

  Model load(std::string const& filename, vertex_data::attrib_flag_t import_attribs) const;
  void store(std::string const& filename, vertex_data::attrib_flag_t import_attribs) const;

 private:
  Transferrer* m_transferrer;
  GeometryDatabase* m_database_geo;
  MaterialDatabase* m_database_mat;
  ModelDatabase* m_database_model;
};

#endif