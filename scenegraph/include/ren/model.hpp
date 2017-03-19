#ifndef MODEL_HPP
#define MODEL_HPP

#include <vulkan/vulkan.hpp>
#include "../bbox.hpp"

class Device;
class Transferrer;

class Model {
 public:  
  Model();
  Model(std::vector<std::string> const& geo, std::vector<std::string> const& mat, glm::vec3 const& box_min, glm::vec3 const& box_max, vk::PrimitiveTopology const& topo = vk::PrimitiveTopology::eTriangleList);
  Model(Model && dev);
  Model(Model const&) = delete;

  Model& operator=(Model const&) = delete;
  Model& operator=(Model&& dev);

  Bbox const& getBox() const;

  void swap(Model& dev);

 private:
  std::vector<std::string> m_geometries;
  std::vector<std::string> m_materials;
  vk::PrimitiveTopology m_topology;
  Bbox m_box;

  friend class Renderer;
};

#endif