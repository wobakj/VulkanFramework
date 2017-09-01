#include "ren/application_instance.hpp"

#include "wrap/device.hpp"
#include "wrap/command_pool.hpp"
// #include "transferrer.hpp"

// #include <iostream>

ApplicationInstance::ApplicationInstance()
 :m_device{nullptr}
{}

ApplicationInstance::ApplicationInstance(ApplicationInstance && rhs)
 :ApplicationInstance{}
{
  swap(rhs);
}

ApplicationInstance::ApplicationInstance(Device const& device, CommandPool& pool)
 :m_device(&device)
 ,m_transferrer{new Transferrer{device, pool}}
 ,m_database_geo{*m_transferrer}
 ,m_database_mat{*m_transferrer}
 ,m_database_model{}
 ,m_database_transform{*m_device}
 ,m_database_texture{*m_transferrer}
 ,m_database_light{*m_device}
 ,m_database_camera{*m_device}
 // ,m_model_loader{m_transferrer}
{}

ApplicationInstance& ApplicationInstance::operator=(ApplicationInstance&& rhs) {
  swap(rhs);
  return *this;
}

void ApplicationInstance::swap(ApplicationInstance& rhs) {
  std::swap(m_device, rhs.m_device);
  std::swap(m_transferrer, rhs.m_transferrer);
  std::swap(m_database_geo, rhs.m_database_geo);
  std::swap(m_database_mat, rhs.m_database_mat);
  std::swap(m_database_model, rhs.m_database_model);
  std::swap(m_database_transform, rhs.m_database_transform);
  std::swap(m_database_texture, rhs.m_database_texture);
  std::swap(m_database_light, rhs.m_database_light);
  std::swap(m_database_camera, rhs.m_database_camera);
  // std::swap(m_model_loader, rhs.m_model_loader);
}

Transferrer& ApplicationInstance::transferrer() {
	return *m_transferrer;
}

GeometryDatabase& ApplicationInstance::dbGeometry() {
	return m_database_geo;
}

MaterialDatabase& ApplicationInstance::dbMaterial() {
	return m_database_mat;
}

ModelDatabase& ApplicationInstance::dbModel() {
	return m_database_model;
}

TransformDatabase& ApplicationInstance::dbTransform() {
	return m_database_transform;
}

TextureDatabase& ApplicationInstance::dbTexture() {
  return m_database_texture;
}

LightDatabase& ApplicationInstance::dbLight() {
  return m_database_light;
}

CameraDatabase& ApplicationInstance::dbCamera() {
	return m_database_camera;
}