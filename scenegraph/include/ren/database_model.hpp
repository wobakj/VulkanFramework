#ifndef DATABASE_MODEL_HPP
#define DATABASE_MODEL_HPP

#include "ren/database.hpp"
#include "ren/model.hpp"

class ModelDatabase : public Database<Model> {
 public:
  ModelDatabase();
  // ModelDatabase(Transferrer& transferrer);
  ModelDatabase(ModelDatabase && dev);
  ModelDatabase(ModelDatabase const&) = delete;
  
  ModelDatabase& operator=(ModelDatabase const&) = delete;
  ModelDatabase& operator=(ModelDatabase&& dev);

  // void store(std::string const& tex_path);
 
};

#endif