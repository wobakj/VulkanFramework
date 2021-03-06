#include "visit/visitor_render.hpp"

#include "node/node.hpp"
#include "node/node_model.hpp"
#include "node/node_light.hpp"
#include "node/node_screen.hpp"
#include "node/node_camera.hpp"
#include "node/node_navigation.hpp"

#include <iostream>

RenderVisitor::RenderVisitor() 
: m_frustum() 
{}

// void RenderVisitor::setFrustum(Frustum const & f)
// {
// 	m_frustum = f;
// }

void RenderVisitor::visit(Node * node)
{
	for (auto child : node->getChildren())
	{
		child->accept(*this);
	}
}

void RenderVisitor::visit(ModelNode * node)
{
	std::cout << "visiting node " << node->getName() << std::endl;
	// if (m_frustum.intersects(*node->getBox())) m_toRender.insert(node);
	m_toRender.emplace_back(node);
	visit(reinterpret_cast<Node*>(node));
}

void RenderVisitor::visit(CameraNode * node)
{
	visit(reinterpret_cast<Node*>(node));
}

void RenderVisitor::visit(NavigationNode * node) {
	visit(static_cast<Node*>(node));
}

void RenderVisitor::visit(LightNode * node)
{
	visit(reinterpret_cast<Node*>(node));
}

void RenderVisitor::visit(ScreenNode * node)
{
	visit(reinterpret_cast<Node*>(node));
}
