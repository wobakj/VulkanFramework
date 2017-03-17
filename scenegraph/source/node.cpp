#include "node.hpp"
#include "hit.hpp"
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "scenegraph.hpp"

#include <iostream>
#include <algorithm>

Node::Node()
{
	m_children = std::vector<std::unique_ptr<Node>>();
}

Node::Node(std::string const & name, glm::mat4 const world):
	m_name(name), m_world(world), m_box(Bbox())
{
	m_children = std::vector<std::unique_ptr<Node>>();
}


void Node::setName(std::string const & name)
{
	m_name = name;
}

// void Node::setWorld(glm::mat4 const & world)
// {
// 	m_world = world;
// }

void Node::setLocal(glm::mat4 const & local)
{
	m_local = local;
}

void Node::setBox(Bbox const& box)
{
	m_box = box;
}

void Node::setParent(Node* const p)
{
	m_parent = p;
}

std::string Node::getName() const
{
	return m_name;
}

glm::mat4 Node::getWorld() const
{
	return m_world;
}

glm::mat4 Node::getLocal() const
{
	return m_local;
}

Bbox Node::getBox() const
{
	return m_box;
}

Node * Node::getParent() const
{
	return m_parent;
}

Scenegraph * Node::getScenegraph() const
{
	return m_scenegraph;
}

std::vector<Node*> Node::getChildren()
{
	std::vector<Node*> children; 
	for (auto const& child : m_children)
	{
		children.push_back(child.get());
	}
	return children;
}

bool Node::hasChildren()
{
	return (m_children.size() > 0);
}

void Node::addChild(Node* n)
{
	m_children.push_back(std::unique_ptr<Node>(n));
}

void Node::removeChild(std::unique_ptr<Node> const child)
{
	m_children.erase(std::find(m_children.begin(), m_children.end(), child));
}

void Node::clearChildren()
{
	m_children.clear();
}

void Node::scale(glm::vec3 const & s)
{
	m_local = glm::scale(m_local, s);
}

void Node::rotate(float const angle, glm::vec3 const & r)
{
	m_local = glm::rotate(m_local, angle, r);
}

void Node::translate(glm::vec3 const & t)
{
	m_local = glm::translate(m_local, t);
}
 
std::shared_ptr<Hit> Node::intersectsRay(Ray const & r) const
{
	std::shared_ptr<Hit> h = std::make_shared<Hit>();
	glm::vec3 normals[6] = { glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0), 
		glm::vec3(0, 0, -1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), 
		glm::vec3(0, 0, 1) };
	//check x axis
	float txmin, txmax, tymin, tymax, tzmin, tzmax;

	if (r.getInvDir().x >= 0.0f)
	{
		txmin = (m_box.getMin().x - r.getOrigin().x) * r.getInvDir().x;
		txmax = (m_box.getMax().x - r.getOrigin().x) * r.getInvDir().x;
	}
	else
	{
		txmin = (m_box.getMax().x - r.getOrigin().x) * r.getInvDir().x;
		txmax = (m_box.getMin().x - r.getOrigin().x) * r.getInvDir().x;
	}

	// check y axis
	if (r.getInvDir().y >= 0.0f)
	{
		tymin = (m_box.getMin().y - r.getOrigin().y) * r.getInvDir().y;
		tymax = (m_box.getMax().y - r.getOrigin().y) * r.getInvDir().y;
	}
	else
	{
		tymin = (m_box.getMax().y - r.getOrigin().y) * r.getInvDir().y;
		tymax = (m_box.getMin().y - r.getOrigin().y) * r.getInvDir().y;
	}

	if (txmin > tymax || tymin > txmax)
	{
		h->setNode(nullptr);
		return h;
	}

	// check z axis
	if (r.getInvDir().z >= 0.0f)
	{
		tzmin = (m_box.getMin().z - r.getOrigin().z) * r.getInvDir().z;
		tzmax = (m_box.getMax().z - r.getOrigin().z) * r.getInvDir().z;
	}
	else
	{
		tzmin = (m_box.getMax().z - r.getOrigin().z) * r.getInvDir().z;
		tzmax = (m_box.getMin().z - r.getOrigin().z) * r.getInvDir().z;
	}

	if (txmin > tzmax || tzmin > txmax) 
	{
		h->setNode(nullptr);
		return h;
	}

	float t0, t1;
	int normal_in, normal_out;

	if (txmin > tymin)
	{
		t0 = txmin;
		normal_in= (r.getInvDir().x >= 0.0) ? 0 : 3;
	}

	else
	{
		t0 = tymin;
		normal_in = (r.getInvDir().y >= 0.0) ? 1 : 4;
	}

	if (tzmin > t0)
	{
		t0 = tzmin;
		normal_in = (r.getInvDir().z >= 0.0) ? 2 : 5;
	}

	if (txmax < tymax)
	{
		t1 = txmax;
		normal_out = (r.getInvDir().x >= 0.0) ? 3 : 0;
	}

	else
	{
		t1 = tymax;
		normal_out = (r.getInvDir().y >= 0.0) ? 4 : 1;
	}

	if (tzmax < t1)
	{
		t1 = tzmax;
		normal_out = (r.getInvDir().z >= 0.0) ? 5 : 2;
	}

	if (t0 < t1 && t1 > 0.001f)
	{
		float s = 0.0f;
		if (t0 > 0.001f)
		{
			s = t0;
			h->setNormal(normals[normal_in]);
		}
		else
		{
			s = t1;
			h->setNormal(normals[normal_out]);
		}

		h->setLocal(r.getOrigin() + r.getDir() * s);
		return h;
	}

	h->setNode(nullptr);
	return h;

}

void Node::accept(NodeVisitor &v)
{
	v.visit(this);
}

/*void Node::updateCache()
{
}*/

