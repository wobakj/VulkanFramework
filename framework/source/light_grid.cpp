#include "light_grid.hpp"

LightGrid::LightGrid(float near,
                     float far,
                     glm::mat4 const& projection,
                     glm::uvec2 const& resolution)
    : m_near(near),
      m_far(far),
      m_tileSize(128, 128),
      m_nearFrustumCornersClipSpace{
          glm::vec4(-1.0f, +1.0f, 0.0f, 1.0f),  // bottom left
          glm::vec4(+1.0f, +1.0f, 0.0f, 1.0f),  // bottom right
          glm::vec4(+1.0f, -1.0f, 0.0f, 1.0f),  // top right
          glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f)   // top left
      } {
  computeDepthSliceValues();
  update(projection, resolution);
}

LightGrid::~LightGrid() {}

// returns true if the light grid was updated
bool LightGrid::update(glm::mat4 const& projection,
                       glm::uvec2 const& resolution) {
  // no need to rebuild froxels if the parameters did not actually change
  if ((projection == m_projection) && (resolution == m_resolution))
    return false;

  // store params as members
  m_resolution = resolution;
  m_projection = projection;

  // recompute number of tiles
  m_tileNum.x = (resolution.x + m_tileSize.x - 1) / m_tileSize.x;
  m_tileNum.y = (resolution.y + m_tileSize.y - 1) / m_tileSize.y;
  m_tileNum.z = static_cast<unsigned int>(m_depthSlices.size() - 1);

  // compute near corners of the frustum in view space
  auto invProj = glm::inverse(projection);
  for (unsigned int i = 0; i < 4; ++i) {
    auto corner = invProj * m_nearFrustumCornersClipSpace[i];
    m_nearFrustumCornersViewSpace[i] = glm::vec3(corner) / corner.w;
  }

  // recompute froxel corners
  m_froxelCorners.resize((m_tileNum.x + 1) * (m_tileNum.y + 1) *
                         (m_tileNum.z + 1));
  for (unsigned int tileY = 0; tileY <= m_tileNum.y; ++tileY)
    for (unsigned int tileX = 0; tileX <= m_tileNum.x; ++tileX) {
      auto ray = viewRay(tileX, tileY);
      for (unsigned int slice = 0; slice <= m_tileNum.z; ++slice) {
        float z = -m_depthSlices.at(slice);
        auto index = getFroxelIndex(tileX, tileY, slice);
        m_froxelCorners.at(index) = ray * (z / ray.z);
      }
    }

  return true;
}

glm::uvec3 LightGrid::dimensions() const {
  return m_tileNum;
}

vk::Extent3D LightGrid::extent() const {
  return vk::Extent3D{m_tileNum.x, m_tileNum.y, m_tileNum.z};
}

// from Oculus' Clustered Renderer using Unreal Engine
// (https://github.com/Oculus-VR/UnrealEngine/blob/4.12-ofr/Engine/Shaders/ClusteredLightGridInjection.usf#L60)
float LightGrid::pointFroxelDistance(unsigned int tileX,
                                     unsigned int tileY,
                                     unsigned int slice,
                                     glm::vec3 const& point) const {
  // read froxel corners
  auto topLeftNear = getFroxelCorner(tileX, tileY + 1, slice);
  auto bottomRightNear = getFroxelCorner(tileX + 1, tileY, slice);
  auto topLeftFar = getFroxelCorner(tileX, tileY + 1, slice + 1);
  auto bottomRightFar = getFroxelCorner(tileX + 1, tileY, slice + 1);

  auto center = (topLeftNear + bottomRightFar) * 0.5f;
  glm::vec3 planeNormal = glm::normalize(center - point);

  float min1 = -dot(planeNormal, point);
  float min2 = min1;

  // factors out a ton of common terms.
  min1 += glm::min(planeNormal.x * topLeftNear.x,
                   planeNormal.x * bottomRightNear.x);
  min1 += glm::min(planeNormal.y * topLeftNear.y,
                   planeNormal.y * bottomRightNear.y);
  min1 += planeNormal.z * topLeftNear.z;
  min2 +=
      glm::min(planeNormal.x * topLeftFar.x, planeNormal.x * bottomRightFar.x);
  min2 +=
      glm::min(planeNormal.y * topLeftFar.y, planeNormal.y * bottomRightFar.y);
  min2 += planeNormal.z * bottomRightFar.z;

  return glm::min(min1, min2);
}

bool LightGrid::sphereFroxelAABBTest(unsigned int tileX,
                                     unsigned int tileY,
                                     unsigned int slice,
                                     glm::vec3 const& center,
                                     float radius) const {
  // read froxel corners
  auto topLeftNear = getFroxelCorner(tileX, tileY + 1, slice);
  auto bottomRightNear = getFroxelCorner(tileX + 1, tileY, slice);
  auto topLeftFar = getFroxelCorner(tileX, tileY + 1, slice + 1);
  auto bottomRightFar = getFroxelCorner(tileX + 1, tileY, slice + 1);

  // setup axis-aligned bounding boxes
  auto froxelAABBMin = glm::vec3(glm::min(topLeftNear.x, topLeftFar.x),
                                 glm::min(bottomRightNear.y, bottomRightFar.y),
                                 glm::min(topLeftFar.z, bottomRightFar.z));
  auto froxelAABBMax = glm::vec3(glm::min(bottomRightNear.x, bottomRightFar.x),
                                 glm::min(topLeftNear.y, topLeftFar.y),
                                 glm::min(topLeftNear.z, bottomRightNear.z));
  auto sphereAABBMin = center - glm::vec3(radius);
  auto sphereAABBMax = center + glm::vec3(radius);

  // check if there is an overlap between the AABBs
  return (((sphereAABBMin.x > froxelAABBMax.x) ||
           (sphereAABBMax.x < froxelAABBMin.x)) ||
          ((sphereAABBMin.y > froxelAABBMax.y) ||
           (sphereAABBMax.y < froxelAABBMin.y)) ||
          ((sphereAABBMin.z > froxelAABBMax.z) ||
           (sphereAABBMax.z < froxelAABBMin.z)));
}

void LightGrid::computeDepthSliceValues() {
  // absolute values from Emil Persson's "Practical Clustered Shading"
  // transformed into (0, 1) range
  // (http://www.humus.name/Articles/PracticalClusteredShading.pdf)
  std::vector<float> relativeDepthSlices = {0.0,
                                            0.009801960392078417,
                                            0.013402680536107223,
                                            0.01820364072814563,
                                            0.02500500100020004,
                                            0.03400680136027206,
                                            0.04620924184836967,
                                            0.0628125625125025,
                                            0.08561712342468493,
                                            0.11642328465693139,
                                            0.15823164632926587,
                                            0.21584316863372677,
                                            0.29185837167433487,
                                            0.3978795759151831,
                                            0.5419083816763353,
                                            0.7359471894378876,
                                            1.0};

  m_depthSlices.clear();
  for (auto& relZ : relativeDepthSlices)
    m_depthSlices.push_back(m_near + (m_far - m_near) * relZ);
}

glm::vec3 LightGrid::viewRay(unsigned int tileX, unsigned int tileY) const {
  auto t = glm::vec2(tileX * m_tileSize.x / static_cast<float>(m_resolution.x),
                     tileY * m_tileSize.y / static_cast<float>(m_resolution.y));

  auto ray = glm::mix(
      // linear interpolation between upper left and right corner of the
      // frustum's near plane
      glm::mix(m_nearFrustumCornersViewSpace[0],
               m_nearFrustumCornersViewSpace[1], t.x),
      // linear interpolation between lower left and right corner of the
      // frustum's near plane
      glm::mix(m_nearFrustumCornersViewSpace[3],
               m_nearFrustumCornersViewSpace[2], t.x),
      // linear interpolation between these two points in y-direction
      t.y);

  return glm::normalize(ray);
}

glm::vec3 LightGrid::getFroxelCorner(unsigned int tileX,
                                     unsigned int tileY,
                                     unsigned int slice) const {
  return m_froxelCorners.at(getFroxelIndex(tileX, tileY, slice));
}

unsigned int LightGrid::getFroxelIndex(unsigned int tileX,
                                       unsigned int tileY,
                                       unsigned int slice) const {
  return slice * ((m_tileNum.x + 1) * (m_tileNum.y + 1)) +
         tileY * (m_tileNum.x + 1) + tileX;
}
