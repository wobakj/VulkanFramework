#ifndef CAMERA_HPP
#define CAMERA_HPP

//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// use floats and med precision operations
#include <glm/gtc/type_precision.hpp>
#include "frustum_2.hpp"

class Camera {
 public:
  Camera(float fov, std::size_t width, std::size_t height, float near_z, float far_z, GLFWwindow* w = nullptr);

  void update(float delta_time);
  void setAspect(std::size_t width, std::size_t height);

  glm::fmat4 const& viewMatrix() const;
  glm::fmat4 const& projectionMatrix() const;
  glm::fvec3 position() const;
  glm::fvec2 const& fov() const;
  float near() const;
  float far() const;
  Frustum2 const& frustum() const;
  bool changed() const;

 private:
  void queryKeyboard(float delta_time);
  void queryMouse(float delta_time);

  // the rendering window
  GLFWwindow* window;
  //variables for mouse transformation input
  double last_x_;
  double last_y_;
  // camera movement to apply in frame
  glm::fvec3 movement_;
  // camera position
  glm::fvec3 position_;
  glm::fvec2 rotation_;
  // world transformation
  glm::fmat4 transformation_;
  // inverted transformation
  glm::fmat4 view_matrix_;
  // whether 1stperson cma is on
  bool flying_cam_;
  // projection parameters
  glm::fmat4 projection_matrix_;
  // the static fov
  float fov_y_;
  // the dynamic fov, depending on aspect
  glm::fvec2 fov_;
  float z_near_;
  float z_far_;
  mutable bool m_changed;
  static const float translation_speed;
  static const float rotation_speed;
  Frustum2 m_frustum;
};

#endif