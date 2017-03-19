#ifndef MODEL_LOADER_HPP
#define MODEL_LOADER_HPP

#include "vertex_data.hpp"

#include <vector>
#include <string>

class ApplicationInstance;
class Model;

class ModelLoader {
 public:  
  ModelLoader();
  ModelLoader(ApplicationInstance& instance);
  ModelLoader(ModelLoader && dev);
  ModelLoader(ModelLoader const&) = delete;

  ModelLoader& operator=(ModelLoader const&) = delete;
  ModelLoader& operator=(ModelLoader&& dev);

  void swap(ModelLoader& dev);

  Model load(std::string const& filename, vertex_data::attrib_flag_t import_attribs) const;
  void store(std::string const& filename, vertex_data::attrib_flag_t import_attribs) const;
  
 private:
  ApplicationInstance* m_instance;
};

#endif