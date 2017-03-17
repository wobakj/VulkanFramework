#include "visitor_render.hpp"
#include "node.hpp"
#include "node_geometry.hpp"
#include "node_light.hpp"
#include "node_Screen.hpp"
#include "node_camera.hpp"
#include "frustum.hpp"

#include <iostream>

RenderVisitor::RenderVisitor()
{
}


RenderVisitor::~RenderVisitor()
{
}

void RenderVisitor::setFrustum(Frustum const & f)
{
	m_frustum = f;
}

void RenderVisitor::visit(Node * node)
{
	for (auto child : node->getChildren())
	{
		child->accept(*this);
	}
}

void RenderVisitor::visit(GeometryNode * node)
{
	std::cout << "\n visiting node " << node->getName();
	if (m_frustum.intersects(*node->getBox())) m_toRender.insert(node);
	for (auto child : node->getChildren())
	{
		child->accept(*this);
	}
}

void RenderVisitor::visit(CameraNode * node)
{
	for (auto child : node->getChildren())
	{
		child->accept(*this);
	}
}

void RenderVisitor::visit(LightNode * node)
{
	for (auto child : node->getChildren())
	{
		child->accept(*this);
	}
}

void RenderVisitor::visit(ScreenNode * node)
{
	for (auto child : node->getChildren())
	{
		child->accept(*this);
	}
}
