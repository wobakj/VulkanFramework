#ifndef NODENAVIGATION_HPP
#define NODENAVIGATION_HPP

#include "node.hpp"
#include <glm/gtc/type_precision.hpp>

class GLFWwindow;

class NavigationNode : public Node
{
public:
	NavigationNode(std::string const& name, GLFWwindow* w);

	void accept(NodeVisitor &v) override;
  
  glm::fvec3 position() const;
  bool changed() const;
  void update(float delta_time);
  void update();
  
 private:
  void queryKeyboard(float delta_time);
  void queryMouse(float delta_time);

  GLFWwindow* m_window;
  //variables for mouse transformation input
  double m_last_x;
  double m_last_y;
  // camera movement to apply in frame
  glm::fvec3 m_movement;
  // camera position
  glm::fvec3 m_position;
  glm::fvec2 m_rotation;
  // inverted transformation
  glm::fmat4 m_view_matrix;
  // whether 1stperson cma is on
  bool m_flying_cam;
  mutable bool m_changed;

  // constant vars
  static const float s_translation_speed;
  static const float s_rotation_speed;
};

#endif
