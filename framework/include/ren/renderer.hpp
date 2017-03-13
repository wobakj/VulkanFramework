#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "ren/node.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Transferrer;
class GeometryDatabase;
class MaterialDatabase;
class ModelDatabase;
class TransformDatabase;
class CommandBuffer;

class Renderer {
 public:  
  Renderer();
  Renderer(MaterialDatabase& mat, GeometryDatabase& geo, ModelDatabase& model, TransformDatabase& trans);
  Renderer(Renderer && dev);
  Renderer(Renderer const&) = delete;

  Renderer& operator=(Renderer const&) = delete;
  Renderer& operator=(Renderer&& dev);

  void swap(Renderer& dev);
  void draw(CommandBuffer& buffer, std::vector<Node const*> const& nodes);

 private:
  GeometryDatabase* m_database_geo;
  MaterialDatabase* m_database_mat;
  ModelDatabase* m_database_model;
  TransformDatabase* m_database_transform;
};

#endif