#include "ren/database_model.hpp"

// #include "ren/model_loader.hpp"

ModelDatabase::ModelDatabase()
 :Database{}
{}

ModelDatabase::ModelDatabase(ModelDatabase && rhs)
 :ModelDatabase{}
{
  swap(rhs);
}

// ModelDatabase::ModelDatabase(Transferrer& transferrer)
//  :Database{transferrer}
// {}

ModelDatabase& ModelDatabase::operator=(ModelDatabase&& rhs) {
  swap(rhs);
  return *this;
}