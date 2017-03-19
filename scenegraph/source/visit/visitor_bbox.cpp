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
 ,m_box()
{
}

Bbox const& BboxVisitor::getBox() const
{
	return m_box;
}

void BboxVisitor::visit(Node * node)
{
	for (auto child : node->getChildren()) {
		child->accept(*this);
	}
}

void BboxVisitor::visit(ModelNode * node)
{
	visit(static_cast<Node*>(node));

	glm::fvec3 currmin{0.0f};
	glm::fvec3 currmax{0.0f}; 
	// auto currmin = node->getOrientedBox().getMin();
	// auto currmax = node->getOrientedBox().getMax(); 

	if (currmin.x < m_box.getMin().x) m_box.setMin(glm::vec3(currmin.x, m_box.getMin().y, m_box.getMin().z));
	if (currmin.y < m_box.getMin().y) m_box.setMin(glm::vec3(m_box.getMin().x, currmin.y, m_box.getMin().z));
	if (currmin.z < m_box.getMin().z) m_box.setMin(glm::vec3(m_box.getMin().x, m_box.getMin().y, currmin.z));

	if (currmax.x < m_box.getMin().x) m_box.setMin(glm::vec3(currmax.x, m_box.getMin().y, m_box.getMin().z));
	if (currmax.y < m_box.getMin().y) m_box.setMin(glm::vec3(m_box.getMin().x, currmax.y, m_box.getMin().z));
	if (currmax.z < m_box.getMin().z) m_box.setMin(glm::vec3(m_box.getMin().x, m_box.getMin().y, currmax.z));

	if (currmin.x > m_box.getMax().x) m_box.setMax(glm::vec3(currmin.x, m_box.getMax().y, m_box.getMax().z));
	if (currmin.y > m_box.getMax().y) m_box.setMax(glm::vec3(m_box.getMax().x, currmin.y, m_box.getMax().z));
	if (currmin.z > m_box.getMax().z) m_box.setMax(glm::vec3(m_box.getMax().x, m_box.getMax().y, currmin.z));

	if (currmax.x > m_box.getMax().x) m_box.setMax(glm::vec3(currmax.x, m_box.getMax().y, m_box.getMax().z));
	if (currmax.y > m_box.getMax().y) m_box.setMax(glm::vec3(m_box.getMax().x, currmax.y, m_box.getMax().z));
	if (currmax.z > m_box.getMax().z) m_box.setMax(glm::vec3(m_box.getMax().x, m_box.getMax().y, currmax.z));

	for (auto child : node->getChildren())
	{
		currmin = child->getBox().getMin();
		currmax = child->getBox().getMax(); 

		if (currmin.x < m_box.getMin().x) m_box.setMin(glm::vec3(currmin.x, m_box.getMin().y, m_box.getMin().z));
		if (currmin.y < m_box.getMin().y) m_box.setMin(glm::vec3(m_box.getMin().x, currmin.y, m_box.getMin().z));
		if (currmin.z < m_box.getMin().z) m_box.setMin(glm::vec3(m_box.getMin().x, m_box.getMin().y, currmin.z));

		if (currmax.x < m_box.getMin().x) m_box.setMin(glm::vec3(currmax.x, m_box.getMin().y, m_box.getMin().z));
		if (currmax.y < m_box.getMin().y) m_box.setMin(glm::vec3(m_box.getMin().x, currmax.y, m_box.getMin().z));
		if (currmax.z < m_box.getMin().z) m_box.setMin(glm::vec3(m_box.getMin().x, m_box.getMin().y, currmax.z));

		if (currmin.x > m_box.getMax().x) m_box.setMax(glm::vec3(currmin.x, m_box.getMax().y, m_box.getMax().z));
		if (currmin.y > m_box.getMax().y) m_box.setMax(glm::vec3(m_box.getMax().x, currmin.y, m_box.getMax().z));
		if (currmin.z > m_box.getMax().z) m_box.setMax(glm::vec3(m_box.getMax().x, m_box.getMax().y, currmin.z));

		if (currmax.x > m_box.getMax().x) m_box.setMax(glm::vec3(currmax.x, m_box.getMax().y, m_box.getMax().z));
		if (currmax.y > m_box.getMax().y) m_box.setMax(glm::vec3(m_box.getMax().x, currmax.y, m_box.getMax().z));
		if (currmax.z > m_box.getMax().z) m_box.setMax(glm::vec3(m_box.getMax().x, m_box.getMax().y, currmax.z));
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
