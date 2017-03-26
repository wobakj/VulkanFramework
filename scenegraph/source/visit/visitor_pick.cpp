#include "visit/visitor_pick.hpp"
#include "hit.hpp"
#include "ray.hpp"
#include "node/node.hpp"
#include "node/node_model.hpp"
#include "node/node_light.hpp"
#include "node/node_camera.hpp"
#include "node/node_screen.hpp"
#include "node/node_navigation.hpp"

#include "ren/application_instance.hpp"


PickVisitor::PickVisitor(ApplicationInstance& instance, Ray const& ray)
 :NodeVisitor()
 ,m_instance(&instance)
 ,m_ray{ray}
{}


void PickVisitor::setRay(Ray const & r)
{
	m_ray = r;
}

std::vector<Hit> const& PickVisitor::getHits() const
{
	return m_hits;
}

void PickVisitor::visit(Node * node)
{
	auto hit = node->getBox().intersects(m_ray);
	if (hit.success()) 
	{
		for (auto child : node->getChildren())
		{
			child->accept(*this);
		}
	}
}

void PickVisitor::visit(ModelNode * node)
{
	auto hierarchy_hit = node->getBox().intersects(m_ray);
	if (hierarchy_hit.success()) {
		// calculate ray in model space
		Ray ray_local = m_ray.transform(glm::inverse(node->getWorld()));
		// intersect model-space bbox
		auto curr_box = m_instance->dbModel().get(node->m_model).getBox();
		auto local_hit = curr_box.intersects(ray_local);
		if (local_hit.success())
		{
			// std::cout<<"local hit "<<node->getName() << std::endl;
			local_hit.setNode(node);
			local_hit.setWorld(node->getWorld() * glm::fvec4{local_hit.getLocal(),1.0f});
			m_hits.push_back(local_hit);
		}
		for (auto child : node->getChildren())
		{
			child->accept(*this);
		}
	}
}

void PickVisitor::visit(CameraNode * node)
{
	visit(static_cast<Node*>(node));
}


void PickVisitor::visit(NavigationNode * node) {
	visit(static_cast<Node*>(node));
}

void PickVisitor::visit(LightNode * node)
{
	visit(static_cast<Node*>(node));
}

void PickVisitor::visit(ScreenNode * node)
{
	visit(static_cast<Node*>(node));
}
