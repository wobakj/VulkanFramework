#include "scenegraph.hpp"
#include "Node_transform.hpp"
#include "Node_camera.hpp"
#include "visitor_Node.hpp"

#include <iostream>

Scenegraph::Scenegraph()
{
}

Scenegraph::Scenegraph(std::string name)
{
	m_name = name;
	m_root = std::make_shared<TransformNode>();
	m_root->setName("root");
}


Scenegraph::~Scenegraph()
{
}

std::shared_ptr<TransformNode> Scenegraph::addTransfNode(std::shared_ptr<Node> parent, std::string name)
{
	auto n = std::make_shared<TransformNode>(name, glm::mat4());
	parent->addChild(n);
	n->setParent(parent);
	return n;
}

void Scenegraph::removeNode(std::shared_ptr<Node> n)
{
	auto foundNode = findNode(n->getName());
	auto parent = foundNode->getParent();
}

std::shared_ptr<Node> Scenegraph::findNode(std::string name)
{
	std::vector<std::shared_ptr<Node>> visited;
	visited.push_back(m_root);
	while (!visited.empty())
	{
		auto current = visited.back();
		visited.pop_back();
		if (current->getName() == name) return current;
		for (auto child : current->getChildren())
		{
			visited.push_back(child);
		}
	}
	return nullptr;
}

bool Scenegraph::hasChildren(std::shared_ptr<Node> n)
{
	return m_root->hasChildren();
}

void Scenegraph::addCamNode(std::shared_ptr<CameraNode> cam)
{
	m_cam_nodes.push_back(cam);
}

std::string Scenegraph::getName() const
{
	return m_name;
}

std::shared_ptr<TransformNode> Scenegraph::getRoot() const
{
	return m_root;
}

void Scenegraph::accept(NodeVisitor & v) const
{
	m_root->accept(v);
}
