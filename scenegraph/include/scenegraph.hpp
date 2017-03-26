#ifndef SCENEGRAPH_HPP
#define SCENEGRAPH_HPP

#include "ren/model_loader.hpp"
#include "node/node_camera.hpp"

#include <memory>
#include <string>
#include <vector>
class Node;
class TransformNode;
class CameraNode;
class NodeVisitor;
class ApplicationInstance;
struct light_t;
struct Camera;

class Scenegraph
{
public:
	Scenegraph();
	Scenegraph(std::string name, ApplicationInstance & instance);

	std::unique_ptr<Node> createCameraNode(std::string const& name, Camera const& cam);
	std::unique_ptr<Node> createGeometryNode(std::string const& name, std::string const& path);
	std::unique_ptr<Node> createLightNode(std::string const& name, light_t const& light);

	void removeNode(std::unique_ptr<Node> n);
	Node* findNode(std::string const& name);
	bool hasChildren(std::unique_ptr<Node> n);
	void addCamNode(CameraNode* cam);

	std::string const& getName() const;
	Node* getRoot() const;
	void accept(NodeVisitor &v) const;

private:
	std::string m_name;
	ApplicationInstance* m_instance;
	ModelLoader m_model_loader;
	std::unique_ptr<Node> m_root;
	std::vector<std::unique_ptr<CameraNode>> m_cam_nodes;
};

#endif
