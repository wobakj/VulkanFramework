#ifndef APPLICATION_INSTANCE_HPP
#define APPLICATION_INSTANCE_HPP

#include "ren/material_database.hpp"
#include "ren/geometry_database.hpp"
#include "ren/model_database.hpp"
#include "ren/transform_database.hpp"
#include "ren/texture_database.hpp"
#include "ren/light_database.hpp"
#include "ren/database_camera.hpp"
// #include "ren/model_loader.hpp"
#include "transferrer.hpp"

#include <memory>

class Device;
class CommandPool;

class ApplicationInstance {
 public:  
  ApplicationInstance();
  ApplicationInstance(Device const& device, CommandPool& pool);
  ApplicationInstance(ApplicationInstance && dev);
  ApplicationInstance(ApplicationInstance const&) = delete;

  ApplicationInstance& operator=(ApplicationInstance const&) = delete;
  ApplicationInstance& operator=(ApplicationInstance&& dev);

  void swap(ApplicationInstance& dev);

  Transferrer& transferrer();
  GeometryDatabase& dbGeometry();
  MaterialDatabase& dbMaterial();
  ModelDatabase& dbModel();
  TransformDatabase& dbTransform();
  TextureDatabase& dbTexture();
  LightDatabase& dbLight();
  CameraDatabase& dbCamera();

 private:
  Device const* m_device;
  std::unique_ptr<Transferrer> m_transferrer;
  GeometryDatabase m_database_geo;
  MaterialDatabase m_database_mat;
  ModelDatabase m_database_model;
  TransformDatabase m_database_transform;
  TextureDatabase m_database_texture;
  LightDatabase m_database_light;
  CameraDatabase m_database_camera;
  // ModelLoader m_model_loader;
};

#endif