#include "scenegraph.hpp"

#include "light.hpp"
#include "camera.hpp"
#include "node/node_model.hpp"
#include "node/node_light.hpp"
// #include "geometry.hpp"
#include "node/node_camera.hpp"
#include "visit/visitor_node.hpp"
#include "ren/application_instance.hpp"

#include <iostream>

Scenegraph::Scenegraph() 
 :m_instance{nullptr}
{}

Scenegraph::Scenegraph(std::string name, ApplicationInstance& instance)
 :m_name{name}
 ,m_instance{&instance}
 ,m_model_loader{*m_instance}
 ,m_root{std::unique_ptr<Node>(new Node(std::string{"root"}, glm::mat4{1.0f}))}
{}

std::unique_ptr<Node> Scenegraph::createGeometryNode(std::string const& name, std::string const& path) {
	if (!m_instance->dbModel().contains(path)) {
  	m_model_loader.store(path, vertex_data::NORMAL | vertex_data::TEXCOORD);
	}
  std::string name_transform{path + "|" + name};
  m_instance->dbTransform().store(name_transform, glm::fmat4{1.0f});
  return std::unique_ptr<Node>(new ModelNode{name, path, name_transform});
}

std::unique_ptr<Node> Scenegraph::createLightNode(std::string const& name, light_t const& light) {
  m_instance->dbLight().store(name, light_t{light});
  return std::unique_ptr<Node>(new LightNode{name, name});
}

std::unique_ptr<Node> Scenegraph::createCameraNode(std::string const& name, Camera const& cam) {
  m_instance->dbCamera().store(name, Camera{cam});
  return std::unique_ptr<Node>(new CameraNode{name, name});
}

void Scenegraph::removeNode(std::unique_ptr<Node> n)
{
	//auto foundNode = findNode(n->getName());
	//auto parent = foundNode->getParent();
}

Node* Scenegraph::findNode(std::string const& name)
{
	std::vector<Node*> visited;
	visited.push_back(m_root.get());
	while (!visited.empty())
	{
		auto& current = visited.back();
		visited.pop_back();
		if (current->getName() == name) {
      return current;
    }
		for (auto const& child : current->getChildren()) {
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
	v.visit(m_root.get());
}
