#include "visit/visitor_pick.hpp"
#include "hit.hpp"
#include "ray.hpp"
#include "node/node.hpp"
#include "node/node_model.hpp"
#include "node/node_light.hpp"
#include "node/node_camera.hpp"
#include "node/node_screen.hpp"

#include "ren/application_instance.hpp"


PickVisitor::PickVisitor(ApplicationInstance& instance) : 
NodeVisitor(), 
m_instance(&instance)
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
	// auto world = node->getWorld();
	// std::cout<<"\naaaaaaaa";
	// for (unsigned int i = 0; i < 4; ++i)
	// {
	// 	for (unsigned int j = 0; j < 4; ++j)
	// 	{
	// 		std::cout<<"\n"<<world[i][j];
	// 	}
	// }
	// std::cout<<"\nbbbbbbbbb";
	std::cout<<"\naaaaaaaa";
	std::cout<<"\n"<<" bmin: "<<node->getBox().getMin().x<<" "<<node->getBox().getMin().y<<" "<<node->getBox().getMin().z<<" bmax: "<<node->getBox().getMax().x<<" "<<node->getBox().getMax().y<<" "<<node->getBox().getMax().z;
	std::cout<<"\nbbbbbbbbb";

	auto hit = node->getBox().intersects(m_ray);
	std::cout<<"\n"<<node->getName()<<" hits " << hit.success();
	if (hit.success()) 
	{
		//hit.setNode(node);
		//m_hits.push_back(hit);
		std::cout<<"\nhierarchy hit "<<node->getName();
		for (auto child : node->getChildren())
		{
			child->accept(*this);
		}
	}
	
}

void PickVisitor::visit(ModelNode * node)
{
	auto hierarchy_hit = node->getBox().intersects(m_ray);
	if (hierarchy_hit.success()) 
	{
		auto curr_box = m_instance->dbModel().get(node->m_model).getBox();
		m_ray.setOrigin(m_ray.getOrigin() * glm::inverse(node->getWorld()));
		m_ray.setDir(m_ray.getDir() * glm::inverse(node->getWorld()));
		auto local_hit = curr_box.intersects(m_ray);
		if (local_hit.success())
		{
			std::cout<<"\nlocal hit "<<node->getName();
			local_hit.setNode(node);
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


void PickVisitor::visit(LightNode * node)
{
	visit(static_cast<Node*>(node));
}

void PickVisitor::visit(ScreenNode * node)
{
	visit(static_cast<Node*>(node));
}
