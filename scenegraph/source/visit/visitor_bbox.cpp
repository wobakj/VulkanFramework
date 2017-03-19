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

	auto curr_box = Bbox();
	if (node->hasChildren()) {
		auto const& children = node->getChildren(); 
		curr_box = children[0]->getBox();
		for (size_t i = 1; i < children.size(); ++i)
		{
			curr_box.join(children[i]->getBox());
		}
	}
	node->setBox(curr_box);
}

void BboxVisitor::visit(ModelNode * node)
{
	for (auto child : node->getChildren()) {
		child->accept(*this);
	}

	auto curr_box = m_instance->dbModel().get(node->m_model).getBox();
	
	std::cout<<"\n"<<node->getName()<<" min: "<<curr_box.getMin().x<<" "<<curr_box.getMin().y<<" "<<curr_box.getMin().z<<" max: "<<curr_box.getMax().x<<" "<<curr_box.getMax().y<<" "<<curr_box.getMax().z;
	// curr_box.transformBox(glm::mat4());
	curr_box.transformBox(node->getWorld());
	
	std::cout<<"\n"<<node->getName()<<" min: "<<curr_box.getMin().x<<" "<<curr_box.getMin().y<<" "<<curr_box.getMin().z<<" max: "<<curr_box.getMax().x<<" "<<curr_box.getMax().y<<" "<<curr_box.getMax().z;
	// std::cout<<"\n"<<node->getName()<<" bmin: "<<curr_box.getMin().x<<" "<<curr_box.getMin().y<<" "<<curr_box.getMin().z<<" bmax: "<<curr_box.getMax().x<<" "<<curr_box.getMax().y<<" "<<curr_box.getMax().z;

	for (auto child : node->getChildren())
	{
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
