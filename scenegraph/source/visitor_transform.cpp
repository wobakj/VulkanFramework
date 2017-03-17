#include "visitor_transform.hpp"
#include "node.hpp"
#include "node_geometry.hpp"
#include "node_light.hpp"
#include "node_screen.hpp"
#include "node_camera.hpp"


TransformVisitor::TransformVisitor() : NodeVisitor(), m_transf(glm::mat4())
{
}


TransformVisitor::~TransformVisitor()
{
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
