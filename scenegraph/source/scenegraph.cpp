#include "scenegraph.hpp"

#include "node_transform.hpp"
#include "node_geometry.hpp"
// #include "geometry.hpp"
#include "node_camera.hpp"
#include "visitor_node.hpp"
#include "ren/application_instance.hpp"

#include <iostream>

Scenegraph::Scenegraph() 
 :m_instance{nullptr}
{}

Scenegraph::Scenegraph(std::string name, ApplicationInstance& instance)
 :m_name{name}
 ,m_instance{&instance}
 ,m_model_loader{*m_instance}
 ,m_root{std::unique_ptr<Node>(new TransformNode(std::string{"root"}, glm::mat4{1.0f}))}
{}

std::unique_ptr<Node> Scenegraph::createGeometryNode(std::string const& name, std::string const& path) {
  m_model_loader.store(path, vertex_data::NORMAL | vertex_data::TEXCOORD);
  std::string name_transform{path + "|" + name};
  m_instance->dbTransform().store(name_transform, glm::fmat4{1.0f});
  return std::unique_ptr<Node>(new GeometryNode{name, path, name_transform});
}

void Scenegraph::removeNode(std::unique_ptr<Node> n)
{
	auto foundNode = findNode(n->getName());
	auto parent = foundNode->getParent();
}

Node* Scenegraph::findNode(std::string name)
{
	std::vector<Node*> visited;
	visited.push_back(m_root.get());
	while (!visited.empty())
	{
		auto& current = visited.back();
		visited.pop_back();
		if (current->getName() == name) return current;
		for (auto child : current->getChildren())
		{
			visited.push_back(child);
		}
	}
	return nullptr;
}

bool Scenegraph::hasChildren(std::unique_ptr<Node> n)
{
	return m_root->hasChildren();
}

void Scenegraph::addCamNode(CameraNode* cam)
{
	m_cam_nodes.push_back(std::unique_ptr<CameraNode>(cam));
}

std::string const& Scenegraph::getName() const
{
	return m_name;
}

Node* Scenegraph::getRoot() const
{
	return m_root.get();
}

void Scenegraph::accept(NodeVisitor & v) const
{
	m_root->accept(v);
}
