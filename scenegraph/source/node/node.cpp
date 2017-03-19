#include "node/node.hpp"
#include "hit.hpp"
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "scenegraph.hpp"

#include <iostream>
#include <algorithm>

Node::Node()
 :m_parent{nullptr}
{
	m_children = std::vector<std::unique_ptr<Node>>();
}

Node::Node(std::string const & name, glm::mat4 const& world)
 :m_parent{nullptr}
 ,m_name(name)
 ,m_world(world)
 ,m_box(Bbox())
{
	m_children = std::vector<std::unique_ptr<Node>>();
}

void Node::setWorld(glm::mat4 const & world)
{
	m_world = world;
}

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

std::string const& Node::getName() const
{
	return m_name;
}

glm::mat4 const& Node::getWorld() const
{
	return m_world;
}

glm::mat4 const& Node::getLocal() const
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

void Node::addChild(std::unique_ptr<Node>&& n)
{
	n->setParent(this);
	m_children.emplace_back(std::move(n));
}

std::vector<std::unique_ptr<Node>>::iterator Node::findChild(std::string const& name) {
	// search for child
	auto child = std::find_if(m_children.begin(), m_children.end(), [&name](std::unique_ptr<Node> const& a){return a->getName() == name;});
	if (child == m_children.end()) {
		throw std::runtime_error{name + " is no child of '" + m_name + "'"};
	}
	return child;
}

Node* Node::getChild(std::string const& name) {
	return findChild(name)->get();
}

std::unique_ptr<Node> Node::detachChild(std::string const& name_child)
{
	// serach for child
	auto child = findChild(name_child);
	// take ownership and remove
	std::unique_ptr<Node> node{child->release()};
	m_children.erase(child);
	return node;
}

void Node::clearChildren()
{
	m_children.clear();
}

void Node::scale(glm::vec3 const & s)
{
	m_local = glm::scale(glm::fmat4{1.0f}, s) * m_local;
}

void Node::rotate(float const angle, glm::vec3 const & r)
{
	m_local = glm::rotate(glm::fmat4{1.0f}, angle, r) * m_local;
}

void Node::translate(glm::vec3 const & t)
{
	m_local = glm::translate(glm::fmat4{1.0f}, t) * m_local;
}

void Node::accept(NodeVisitor &v)
{
	v.visit(this);
}

/*void Node::updateCache()
{
}*/

