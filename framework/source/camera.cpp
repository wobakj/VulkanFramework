#include "camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

const float Camera::rotation_speed = 0.05f;
const float Camera::translation_speed = 1.0f;

Camera::Camera(float fov, std::size_t width, std::size_t height, float near_z, float far_z, GLFWwindow* w)
   :window{w}
   ,movement_{0.0f}
   ,position_{0.0f, 0.5f, 1.0f}
   ,rotation_{0.0f, 0.0f}
   ,transformation_{0.0f}
   ,view_matrix_{0.0f}
   ,flying_cam_{true}
   ,fov_y_{fov}
   ,fov_{0.0f}
   ,z_near_{near_z}
   ,z_far_{far_z}
  {
    setAspect(width, height);
  }

void Camera::update(float delta_time) {
  queryKeyboard(delta_time);
  queryMouse(delta_time);

  transformation_ = glm::mat4{1.0f};
  // yaw
  transformation_ = glm::rotate(transformation_, rotation_.y, glm::vec3{0.0f, 1.0f, 0.0f});
  // pitch
  transformation_ = glm::rotate(transformation_, rotation_.x, glm::vec3{1.0f, 0.0f, 0.0f});

  if (flying_cam_) {
    // calculate direction camera is facing
    glm::vec3 direction = glm::mat3{transformation_} * movement_;
    position_ += direction;
    // move rotated camera in global space to position
    transformation_[3] += glm::vec4{position_, 0.0f}; 
  }
  else {
    position_ += movement_;
    transformation_ = glm::translate(transformation_, position_);
  }
  //invert the camera transformation to move the vertices
  view_matrix_ = glm::inverse(transformation_);
  // reset applied movement 
  movement_ = glm::vec3{0.0f};
}

void Camera::setAspect(std::size_t width, std::size_t height) {
  float aspect = float(width) / float(height);
  float fov_y = fov_y_;
  // if width is smaller, extend vertical fov 
  if(width < height) {
    fov_y = 2.0f * glm::atan(glm::tan(fov_y_ * 0.5f) * (1.0f / aspect));
  }
  // projection is hor+ 
  projection_matrix_ = glm::perspective(fov_y, aspect, z_near_, z_far_);
  // save current fov
  fov_ = glm::vec2{2.0f * glm::atan(glm::tan(fov_y * 0.5f) * aspect), fov_y};
}

glm::mat4 const& Camera::viewMatrix() const {
  return view_matrix_;
}

glm::mat4 const& Camera::projectionMatrix() const {
  return projection_matrix_;
}

glm::vec3 Camera::position() const {
  return glm::vec3(transformation_[3] / transformation_[3][3]);
}

glm::vec2 const& Camera::fov() const {
  return fov_;
}

void Camera::queryKeyboard(float delta_time) {
  if (glfwGetKey(window, GLFW_KEY_ENTER)) {
    flying_cam_ = !flying_cam_;
    //reset camera transforms
    if(!flying_cam_)
    {
      position_ = glm::vec3{0.0f, 0.0f, glm::length(glm::vec3{view_matrix_[3]})};
      // get polar coordinates of position
      rotation_ = glm::tan(glm::vec2{view_matrix_[3][1] / view_matrix_[3][2], view_matrix_[3][0] / view_matrix_[3][2]});
    }
    else
    {
      rotation_.y = -rotation_.y;
      position_ = -glm::vec3{view_matrix_[3]};
      // rotation_ = glm::vec2{0.0f};
    }
  }

  // forward and backward movement
  if (glfwGetKey(window, GLFW_KEY_W)) {
    movement_.z -= delta_time * translation_speed;
  }
  if (glfwGetKey(window, GLFW_KEY_S)) {
    movement_.z += delta_time * translation_speed;
  }
  // for 1stperson hor movement
  if (flying_cam_) {
    if (glfwGetKey(window, GLFW_KEY_A)) {
      movement_.x -= delta_time * translation_speed;
    }
    if (glfwGetKey(window, GLFW_KEY_D)) {
      movement_.x += delta_time * translation_speed;
    }
  }
}

void Camera::queryMouse(float delta_time) {
  glm::vec2 curr_mouse{0.0f};
  double curr_x = 0.0;
  double curr_y = 0.0;
  glfwGetCursorPos(window, &curr_x, &curr_y);
  
  float relX = float(curr_x - last_x_);
  float relY = float(curr_y - last_y_);

  //inverted yaw for center cam
  if(!flying_cam_) {
   relX = -relX; 
  }
  rotation_.y -= relX * rotation_speed;
  rotation_.x -= relY * rotation_speed;
  // prevent flipping of up-vector
  rotation_.x = glm::clamp(rotation_.x, float(M_PI * -0.5f), float(M_PI * 0.5f));
  //update mouse position
  last_x_ = curr_x;
  last_y_ = curr_y;
}
