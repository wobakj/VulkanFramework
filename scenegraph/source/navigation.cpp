#include "navigation.hpp"
//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

const float Navigation::s_rotation_speed = 0.01f;
const float Navigation::s_translation_speed = 1.0f;

Navigation::Navigation(GLFWwindow* window)
 :m_window{window}
 ,m_local{glm::mat4{1.0f}}
 ,m_movement{0.0f}
 ,m_position{0.0f, 0.5f, 1.0f}
 ,m_rotation{0.0f, 0.0f}
 ,m_view_matrix{0.0f}
 ,m_flying_cam{true}
 ,m_changed{true}
{}

glm::mat4 const& Navigation::getTransform() const
{
  return m_local;
}

void Navigation::setTransform(glm::mat4 const& transform)
{
  m_local = transform;
  m_position = glm::vec3(m_local[3]);
} 

glm::fvec2 const& Navigation::getRotation() const
{
  return m_rotation;
}

void Navigation::update() {
  // ugly static initialisation
  static double time_last = glfwGetTime();
  // calculate delta time
  double time_current = glfwGetTime();
  float time_delta = float(time_current - time_last);
  time_last = time_current;
  update(time_delta);
}

void Navigation::update(float delta_time) {
  queryKeyboard(delta_time);
  queryMouse(delta_time);

  m_local = glm::fmat4{1.0f};
  // yaw
  m_local = glm::rotate(m_local, m_rotation.y, glm::fvec3{0.0f, 1.0f, 0.0f});
  // pitch
  m_local = glm::rotate(m_local, m_rotation.x, glm::fvec3{1.0f, 0.0f, 0.0f});

  if (m_flying_cam) {
    // calculate direction camera is facing
    glm::fvec3 direction = glm::mat3{m_local} * m_movement;
    m_position += direction;
    // move rotated camera in global space to position
    m_local[3] += glm::vec4{m_position, 0.0f}; 
  }
  else {
    m_position += m_movement;
    m_local = glm::translate(m_local, m_position);
  }
  //invert the camera transformation to move the vertices
  m_view_matrix = glm::inverse(m_local);
  // reset applied movement 
  m_movement = glm::fvec3{0.0f};
}

void Navigation::setMovement(glm::fvec3 movement)
{
  m_movement = movement;
}

glm::fvec3 Navigation::position() const {
  return glm::fvec3(m_local[3] / m_local[3][3]);
}

bool Navigation::changed() const {
  bool changed = m_changed;
  m_changed = false;
  return changed;
}

void Navigation::queryKeyboard(float delta_time) {
  if (glfwGetKey(m_window, GLFW_KEY_ENTER)) {
    m_flying_cam = !m_flying_cam;
    //reset camera transforms
    if(!m_flying_cam)
    {
      m_position = glm::fvec3{0.0f, 0.0f, glm::length(glm::fvec3{m_view_matrix[3]})};
      // get polar coordinates of position
      m_rotation = glm::tan(glm::fvec2{m_view_matrix[3][1] / m_view_matrix[3][2], m_view_matrix[3][0] / m_view_matrix[3][2]});
    }
    else
    {
      m_rotation.y = -m_rotation.y;
      m_position = -glm::fvec3{m_view_matrix[3]};
      // m_rotation = glm::fvec2{0.0f};
    }
  }

  // forward and backward movement
  if (glfwGetKey(m_window, GLFW_KEY_W)) {
    m_movement.z -= delta_time * s_translation_speed;
  }
  if (glfwGetKey(m_window, GLFW_KEY_S)) {
    m_movement.z += delta_time * s_translation_speed;
  }
  // for 1stperson hor movement
  if (m_flying_cam) {
    if (glfwGetKey(m_window, GLFW_KEY_A)) {
      m_movement.x -= delta_time * s_translation_speed;
    }
    if (glfwGetKey(m_window, GLFW_KEY_D)) {
      m_movement.x += delta_time * s_translation_speed;
    }
  }

  if (m_movement != glm::fvec3{0.0f}) {
    m_changed = true;
  }
}

void Navigation::queryMouse(float delta_time) {
  glm::fvec2 curr_mouse{0.0f};
  double curr_x = 0.0;
  double curr_y = 0.0;
  glfwGetCursorPos(m_window, &curr_x, &curr_y);
  
  float relX = float(curr_x - m_last_x);
  float relY = float(curr_y - m_last_y);

  //inverted yaw for center cam
  if(!m_flying_cam) {
   relX = -relX; 
  }
  m_rotation.y -= relX * s_rotation_speed;
  m_rotation.x -= relY * s_rotation_speed;
  // prevent flipping of up-vector
  m_rotation.x = glm::clamp(m_rotation.x, float(M_PI * -0.5f), float(M_PI * 0.5f));

  if (m_last_x != curr_x || m_last_y != curr_y) {
    m_changed = true;
  }
  //update mouse position
  m_last_x = curr_x;
  m_last_y = curr_y;
}
