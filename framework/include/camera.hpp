#ifndef CAMERA_HPP
#define CAMERA_HPP

//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// use floats and med precision operations
#include <glm/gtc/type_precision.hpp>

class Camera {
 public:
  Camera(float fov, std::size_t width, std::size_t height, float near_z, float far_z, GLFWwindow* w = nullptr);

  void update(float delta_time);
  void setAspect(std::size_t width, std::size_t height);

  glm::mat4 const& viewMatrix() const;
  glm::mat4 const& projectionMatrix() const;
  glm::vec3 position() const;
  glm::vec2 const& fov() const;

 private:
  void queryKeyboard(float delta_time);
  void queryMouse(float delta_time);

  // the rendering window
  GLFWwindow* window;
  //variables for mouse transformation input
  double last_x_;
  double last_y_;
  // camera movement to apply in frame
  glm::vec3 movement_;
  // camera position
  glm::vec3 position_;
  glm::vec2 rotation_;
  // world transformation
  glm::mat4 transformation_;
  // inverted transformation
  glm::mat4 view_matrix_;
  // whether 1stperson cma is on
  bool flying_cam_;
  // projection parameters
  glm::mat4 projection_matrix_;
  // the static fov
  float fov_y_;
  // the dynamic fov, depending on aspect
  glm::vec2 fov_;
  float z_near_;
  float z_far_;

  static const float translation_speed;
  static const float rotation_speed;
};

#endif