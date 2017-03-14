#include "camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

const float Camera::rotation_speed = 0.01f;
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
   ,m_changed{true}
  {
    setAspect(width, height);
  }

void Camera::update(float delta_time) {
  queryKeyboard(delta_time);
  queryMouse(delta_time);

  transformation_ = glm::fmat4{1.0f};
  // yaw
  transformation_ = glm::rotate(transformation_, rotation_.y, glm::fvec3{0.0f, 1.0f, 0.0f});
  // pitch
  transformation_ = glm::rotate(transformation_, rotation_.x, glm::fvec3{1.0f, 0.0f, 0.0f});

  if (flying_cam_) {
    // calculate direction camera is facing
    glm::fvec3 direction = glm::mat3{transformation_} * movement_;
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
  movement_ = glm::fvec3{0.0f};

  m_frustum.update(projection_matrix_ * view_matrix_);
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
  fov_ = glm::fvec2{2.0f * glm::atan(glm::tan(fov_y * 0.5f) * aspect), fov_y};
  // vulkan clip space correction
  // https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
  const glm::fmat4 clip_matrix{1.0f,  0.0f, 0.0f, 0.0f,
                       0.0f, -1.0f, 0.0f, 0.0f,
                       0.0f,  0.0f, 0.5f, 0.0f,
                       0.0f,  0.0f, 0.5f, 1.0f};
  projection_matrix_ = clip_matrix * projection_matrix_;
}

glm::fmat4 const& Camera::viewMatrix() const {
  return view_matrix_;
}

glm::fmat4 const& Camera::projectionMatrix() const {
  return projection_matrix_;
}

glm::fvec3 Camera::position() const {
  return glm::fvec3(transformation_[3] / transformation_[3][3]);
}

glm::fvec2 const& Camera::fov() const {
  return fov_;
}

float Camera::near() const {
  return z_near_;
}

float Camera::far() const {
  return z_far_;
}

Frustum const& Camera::frustum() const {
  return m_frustum;
}

bool Camera::changed() const {
  bool changed = m_changed;
  m_changed = false;
  return changed;
}


void Camera::queryKeyboard(float delta_time) {
  if (glfwGetKey(window, GLFW_KEY_ENTER)) {
    flying_cam_ = !flying_cam_;
    //reset camera transforms
    if(!flying_cam_)
    {
      position_ = glm::fvec3{0.0f, 0.0f, glm::length(glm::fvec3{view_matrix_[3]})};
      // get polar coordinates of position
      rotation_ = glm::tan(glm::fvec2{view_matrix_[3][1] / view_matrix_[3][2], view_matrix_[3][0] / view_matrix_[3][2]});
    }
    else
    {
      rotation_.y = -rotation_.y;
      position_ = -glm::fvec3{view_matrix_[3]};
      // rotation_ = glm::fvec2{0.0f};
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

  if (movement_ != glm::fvec3{0.0f}) {
    m_changed = true;
  }
}

void Camera::queryMouse(float delta_time) {
  glm::fvec2 curr_mouse{0.0f};
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

  if (last_x_ != curr_x || last_y_ != curr_y) {
    m_changed = true;
  }
  //update mouse position
  last_x_ = curr_x;
  last_y_ = curr_y;
}
