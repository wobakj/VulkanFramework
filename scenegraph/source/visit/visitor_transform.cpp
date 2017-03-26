#include "visit/visitor_transform.hpp"

#include "node/node.hpp"
#include "node/node_model.hpp"
#include "node/node_light.hpp"
#include "node/node_screen.hpp"
#include "node/node_camera.hpp"
#include "node/node_navigation.hpp"

#include "ren/application_instance.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "glm/gtx/string_cast.hpp"

TransformVisitor::TransformVisitor(ApplicationInstance& instance) 
 :NodeVisitor{}
 ,m_instance{&instance}
{}

void TransformVisitor::visit(Node * node) {
	if (node->getParent()) {
		node->setWorld(node->getParent()->getWorld() * node->getLocal());
	}
	else {
		node->setWorld(node->getLocal());
	}

	for (auto child : node->getChildren()) {
		child->accept(*this);
	}
}

void TransformVisitor::visit(ModelNode * node) {
	visit(static_cast<Node*>(node));
  m_instance->dbTransform().set(node->m_transform, node->getWorld());
}

void TransformVisitor::visit(CameraNode * node) {
	visit(static_cast<Node*>(node));
  auto cam = m_instance->dbCamera().get(node->m_cam);
  cam.update(1.0f / 60.0f);
  // m_instance.dbCamera().set("cam", std::move(cam));
  m_instance->dbCamera().set(node->m_cam, std::move(cam));
}

void TransformVisitor::visit(NavigationNode * node) {
	visit(static_cast<Node*>(node));
}

void TransformVisitor::visit(LightNode * node) {
	visit(static_cast<Node*>(node));
	// store new position in buffer and mark for upload
	auto& light = m_instance->dbLight().getEdit(node->id());
	light.position = glm::fvec3(glm::column(node->getWorld(), 3));
}

void TransformVisitor::visit(ScreenNode * node) {
	visit(static_cast<Node*>(node));
}
