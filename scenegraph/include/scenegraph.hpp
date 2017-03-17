#ifndef SCENEGRAPH_HPP
#define SCENEGRAPH_HPP

#include <string>
#include <vector>
#include <memory>

class Node;
class TransformNode;
class CameraNode;
class NodeVisitor;

class Scenegraph
{
public:
	Scenegraph();
	Scenegraph(std::string name);
	~Scenegraph();

	std::shared_ptr<TransformNode> addTransfNode(std::shared_ptr<Node> parent, std::string name);
	void removeNode(std::shared_ptr<Node> n);
	std::shared_ptr<Node> findNode(std::string name);
	bool hasChildren(std::shared_ptr<Node> n);
	void addCamNode(std::shared_ptr<CameraNode> cam);
	

	std::string getName() const;
	std::shared_ptr<TransformNode> getRoot() const;

	void accept(NodeVisitor &v) const;

private:
	std::string m_name;
	std::shared_ptr<TransformNode> m_root;
	std::vector<std::shared_ptr<CameraNode>> m_cam_nodes;
};

#endif
