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
	// first calculate correct child boxes
	for (auto child : node->getChildren()) {
		child->accept(*this);
	}
	// if children exist, use them
	auto curr_box = Bbox();
	if (node->hasChildren()) {
		// take only children nodes for bbox
		auto const& children = node->getChildren(); 
		curr_box = children[0]->getBox();
		for (size_t i = 1; i < children.size(); ++i) {
			curr_box.join(children[i]->getBox());
		}
	}
	node->setBox(curr_box);
}

void BboxVisitor::visit(ModelNode * node)
{
	// first calculate correct child boxes
	for (auto child : node->getChildren()) {
		child->accept(*this);
	}
	// get model-space bbox
	auto curr_box = m_instance->dbModel().get(node->m_model).getBox();
	// transform into world space
	curr_box.transformBox(node->getWorld());
	
	// grow bbox
	for (auto child : node->getChildren()) {
		curr_box.join(child->getBox());
	}
	node->setBox(curr_box);
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
