#ifndef DATABASE_GEOMETRY_HPP
#define DATABASE_GEOMETRY_HPP

#include "ren/database.hpp"
#include "geometry.hpp"

#include <vulkan/vulkan.hpp>

#include <map>

class Device;
class Image;
class Transferrer;

class GeometryDatabase : public Database<Geometry> {
 public:
  GeometryDatabase();
  GeometryDatabase(Transferrer& transferrer);
  GeometryDatabase(GeometryDatabase && dev);
  GeometryDatabase(GeometryDatabase const&) = delete;
  
  GeometryDatabase& operator=(GeometryDatabase const&) = delete;
  GeometryDatabase& operator=(GeometryDatabase&& dev);

  // void swap(GeometryDatabase& dev);
  // void store(std::string const& tex_path) override;
};

#endif