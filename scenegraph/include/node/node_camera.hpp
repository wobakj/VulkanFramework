#ifndef NODECAMERA_HPP
#define NODECAMERA_HPP

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <memory>

#include "scenegraph.hpp"
#include "frustum.hpp"
#include "node.hpp"
#include "hit.hpp"

class CameraNode : public Node
{
public:

  CameraNode();
  CameraNode(std::string const& name, std::string const& cam);

  Frustum makePerspective(Scenegraph const& scene, glm::mat4 cam_transf, glm::mat4 screen_transf);
  void accept(NodeVisitor &v) override;

private:
  std::string m_cam;
	float m_near;
	float m_far;
	glm::vec2 m_resolution;
	unsigned int m_id;
	float m_eye_distance;

  friend class TransformVisitor;
  friend class PickVisitor;
  friend class BboxVisitor;
  friend class RenderVisitor;
  friend class Renderer;
};

#endif

