#include "visit/visitor_transform.hpp"
#include "node.hpp"
#include "node_geometry.hpp"
#include "node_light.hpp"
#include "node_screen.hpp"
#include "node_camera.hpp"

#include "ren/application_instance.hpp"

TransformVisitor::TransformVisitor(ApplicationInstance& instance) 
 :NodeVisitor{}
 ,m_transf{glm::mat4()}
 ,m_instance{&instance}
{}

glm::mat4 const& TransformVisitor::getTransform() const
{
	return m_transf;
}

void TransformVisitor::visit(Node * node)
{
	m_transf = node->getLocal() * m_transf;
	node->setWorld(m_transf);
	for (auto child : node->getChildren())
	{
		child->accept(*this);
	}
}

void TransformVisitor::visit(GeometryNode * node)
{
	m_transf = node->getLocal() * m_transf;
	node->setWorld(m_transf); // world or local?
  m_instance->dbTransform().set(node->m_transform, m_transf);
	for (auto child : node->getChildren())
	{
		child->accept(*this);
	}
}

void TransformVisitor::visit(CameraNode * node)
{
	m_transf = node->getLocal() * m_transf;
	node->setWorld(m_transf);
	for (auto child : node->getChildren())
	{
		child->accept(*this);
	}
}

void TransformVisitor::visit(LightNode * node)
{
	m_transf = node->getLocal() * m_transf;
	node->setWorld(m_transf);
	for (auto child : node->getChildren())
	{
		child->accept(*this);
	}
}

void TransformVisitor::visit(ScreenNode * node)
{
}
