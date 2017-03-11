#ifndef MODEL_DATABASE_HPP
#define MODEL_DATABASE_HPP

#include "ren/database.hpp"
#include "geometry.hpp"

#include <vulkan/vulkan.hpp>

#include <map>

class Device;
class Image;
class Transferrer;

class ModelDatabase : public Database<Geometry> {
 public:
  ModelDatabase();
  ModelDatabase(Transferrer& transferrer);
  ModelDatabase(ModelDatabase && dev);
  ModelDatabase(ModelDatabase const&) = delete;
  
  ModelDatabase& operator=(ModelDatabase const&) = delete;
  ModelDatabase& operator=(ModelDatabase&& dev);

  // void swap(ModelDatabase& dev);
  // void store(std::string const& tex_path) override;
};

#endif