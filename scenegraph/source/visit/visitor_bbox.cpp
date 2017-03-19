#include "visit/visitor_bbox.hpp"

#include "node/node.hpp"
#include "node/node_model.hpp"
#include "node/node_light.hpp"
#include "node/node_screen.hpp"
#include "node/node_camera.hpp"

#include "ren/application_instance.hpp"

BboxVisitor::BboxVisitor(ApplicationInstance& instance) 
 :NodeVisitor() 
 ,m_instance{&instance}
{}

void BboxVisitor::visit(Node * node)
{
	for (auto child : node->getChildren()) {
		child->accept(*this);
	}
}

void BboxVisitor::visit(ModelNode * node)
{
	visit(static_cast<Node*>(node));

	auto curr_box = m_instance->dbModel().get(node->m_model).getBox();
	
	curr_box.transformBox(glm::inverse(node->getLocal()) * node->getWorld());

	for (auto child : node->getChildren())
	{
		curr_box.join(child->getBox());
	}
}

void BboxVisitor::visit(CameraNode * node)
{
	visit(static_cast<Node*>(node));
}

void BboxVisitor::visit(LightNode * node)
{
	visit(static_cast<Node*>(node));
}

void BboxVisitor::visit(ScreenNode * node)
{
	visit(static_cast<Node*>(node));
}
