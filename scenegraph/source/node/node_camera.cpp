#include "node/node_camera.hpp"

#include <iostream>


CameraNode::CameraNode()
 :Node()
{}

CameraNode::CameraNode(std::string const& name, std::string const& cam)
 :Node(name, glm::fmat4{1.0f})
 ,m_cam{cam}
{}

Frustum CameraNode::makePerspective(Scenegraph const& scene, glm::mat4 cam_transf, glm::mat4 screen_transf) {
	Frustum f = Frustum{};
	// f.setNear(m_near);
	// f.setFar(m_far);
	// f.setProj(f.perspective(90.0f, m_resolution, cam_transf, screen_transf, m_near, m_far));
	// f.calculatePlanes(cam_transf, screen_transf, f);
	return f;
}

void CameraNode::accept(NodeVisitor &v) {
	v.visit(this);
}
/*
std::set<hit> const camera_node::rayTest(rayNode const & ray)
{
	return std::set<hit>() ;
}*/
