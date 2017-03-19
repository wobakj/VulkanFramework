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

	// glm::fvec3 currmin{0.0f};
	// glm::fvec3 currmax{0.0f}; 

	auto curr_box = m_instance->dbModel().get(node->m_model).getBox();
	auto currmin = curr_box.getMin();
	auto currmax = curr_box.getMax(); 

	if (currmin.x < curr_box.getMin().x) curr_box.setMin(glm::vec3(currmin.x, curr_box.getMin().y, curr_box.getMin().z));
	if (currmin.y < curr_box.getMin().y) curr_box.setMin(glm::vec3(curr_box.getMin().x, currmin.y, curr_box.getMin().z));
	if (currmin.z < curr_box.getMin().z) curr_box.setMin(glm::vec3(curr_box.getMin().x, curr_box.getMin().y, currmin.z));

	if (currmax.x < curr_box.getMin().x) curr_box.setMin(glm::vec3(currmax.x, curr_box.getMin().y, curr_box.getMin().z));
	if (currmax.y < curr_box.getMin().y) curr_box.setMin(glm::vec3(curr_box.getMin().x, currmax.y, curr_box.getMin().z));
	if (currmax.z < curr_box.getMin().z) curr_box.setMin(glm::vec3(curr_box.getMin().x, curr_box.getMin().y, currmax.z));

	if (currmin.x > curr_box.getMax().x) curr_box.setMax(glm::vec3(currmin.x, curr_box.getMax().y, curr_box.getMax().z));
	if (currmin.y > curr_box.getMax().y) curr_box.setMax(glm::vec3(curr_box.getMax().x, currmin.y, curr_box.getMax().z));
	if (currmin.z > curr_box.getMax().z) curr_box.setMax(glm::vec3(curr_box.getMax().x, curr_box.getMax().y, currmin.z));

	if (currmax.x > curr_box.getMax().x) curr_box.setMax(glm::vec3(currmax.x, curr_box.getMax().y, curr_box.getMax().z));
	if (currmax.y > curr_box.getMax().y) curr_box.setMax(glm::vec3(curr_box.getMax().x, currmax.y, curr_box.getMax().z));
	if (currmax.z > curr_box.getMax().z) curr_box.setMax(glm::vec3(curr_box.getMax().x, curr_box.getMax().y, currmax.z));

	for (auto child : node->getChildren())
	{
		currmin = child->getBox().getMin();
		currmax = child->getBox().getMax(); 

		if (currmin.x < curr_box.getMin().x) curr_box.setMin(glm::vec3(currmin.x, curr_box.getMin().y, curr_box.getMin().z));
		if (currmin.y < curr_box.getMin().y) curr_box.setMin(glm::vec3(curr_box.getMin().x, currmin.y, curr_box.getMin().z));
		if (currmin.z < curr_box.getMin().z) curr_box.setMin(glm::vec3(curr_box.getMin().x, curr_box.getMin().y, currmin.z));

		if (currmax.x < curr_box.getMin().x) curr_box.setMin(glm::vec3(currmax.x, curr_box.getMin().y, curr_box.getMin().z));
		if (currmax.y < curr_box.getMin().y) curr_box.setMin(glm::vec3(curr_box.getMin().x, currmax.y, curr_box.getMin().z));
		if (currmax.z < curr_box.getMin().z) curr_box.setMin(glm::vec3(curr_box.getMin().x, curr_box.getMin().y, currmax.z));

		if (currmin.x > curr_box.getMax().x) curr_box.setMax(glm::vec3(currmin.x, curr_box.getMax().y, curr_box.getMax().z));
		if (currmin.y > curr_box.getMax().y) curr_box.setMax(glm::vec3(curr_box.getMax().x, currmin.y, curr_box.getMax().z));
		if (currmin.z > curr_box.getMax().z) curr_box.setMax(glm::vec3(curr_box.getMax().x, curr_box.getMax().y, currmin.z));

		if (currmax.x > curr_box.getMax().x) curr_box.setMax(glm::vec3(currmax.x, curr_box.getMax().y, curr_box.getMax().z));
		if (currmax.y > curr_box.getMax().y) curr_box.setMax(glm::vec3(curr_box.getMax().x, currmax.y, curr_box.getMax().z));
		if (currmax.z > curr_box.getMax().z) curr_box.setMax(glm::vec3(curr_box.getMax().x, curr_box.getMax().y, currmax.z));
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
