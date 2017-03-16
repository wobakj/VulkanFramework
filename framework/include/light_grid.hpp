#ifndef LIGHT_GRID_HPP
#define LIGHT_GRID_HPP

#include <iostream>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

class LightGrid {
 public:
  LightGrid(float near,
            float far,
            glm::mat4 const& projection,
            glm::uvec2 const& resolution);
  ~LightGrid();

  bool update(glm::mat4 const& projection, glm::uvec2 const& resolution);

  glm::uvec3 recomputeDimensions(glm::uvec2 const& resolution);
  glm::uvec3 dimensions() const;
  vk::Extent3D extent() const;

  float pointFroxelDistance(unsigned int tileX,
                            unsigned int tileY,
                            unsigned int slice,
                            glm::vec3 const& point) const;
  bool sphereFroxelAABBTest(unsigned int tileX,
                            unsigned int tileY,
                            unsigned int slice,
                            glm::vec3 const& center,
                            float radius) const;

 private:
  void computeDepthSliceValues();
  glm::vec3 viewRay(unsigned int tileX, unsigned int tileY) const;
  glm::vec3 getFroxelCorner(unsigned int tileX,
                            unsigned int tileY,
                            unsigned int slice) const;
  unsigned int getFroxelIndex(unsigned int tileX,
                              unsigned int tileY,
                              unsigned int slice) const;

 private:
  float m_near;
  float m_far;
  glm::uvec2 m_resolution;
  glm::mat4 m_projection;
  glm::mat4 m_view;
  glm::uvec2 m_tileSize;
  glm::uvec3 m_tileNum;
  std::vector<float> m_depthSlices;
  std::array<glm::vec4, 4> m_nearFrustumCornersClipSpace;
  std::array<glm::vec3, 4> m_nearFrustumCornersViewSpace;
  std::vector<glm::vec3> m_froxelCorners;
};

#endif  // LIGHT_GRID_HPP