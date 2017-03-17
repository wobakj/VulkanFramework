#include "visit/visitor_pick.hpp"
#include "hit.hpp"
#include "ray.hpp"
#include "node.hpp"
#include "node_geometry.hpp"
#include "node_light.hpp"
#include "node_camera.hpp"
#include "node_screen.hpp"


PickVisitor::PickVisitor() : NodeVisitor()
{
}

PickVisitor::~PickVisitor()
{
}

void PickVisitor::setRay(Ray const & r)
{
	m_ray = r;
}

std::vector<std::shared_ptr<Hit>> const& PickVisitor::getHits() const
{
	return m_hits;
}

void PickVisitor::visit(Node * node)
{
	auto hits = node->intersectsRay(m_ray);
	m_hits.push_back(hits);
	for (auto child : node->getChildren())
	{
		child->accept(*this);
	}
}

void PickVisitor::visit(GeometryNode * node)
{
	auto hits = node->intersectsRay(m_ray);
	m_hits.push_back(hits);
	for (auto child : node->getChildren())
	{
		child->accept(*this);
	}
}

void PickVisitor::visit(CameraNode * node)
{
}

void PickVisitor::visit(LightNode * node)
{
}

void PickVisitor::visit(ScreenNode * node)
{
}
