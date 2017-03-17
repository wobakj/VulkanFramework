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

	void removeNode(std::unique_ptr<Node> n);
	Node* findNode(std::string name);
	bool hasChildren(std::unique_ptr<Node> n);
	void addCamNode(CameraNode* cam);

	std::string getName() const;
	Node* getRoot() const;
	void accept(NodeVisitor &v) const;

private:
	std::string m_name;
	std::unique_ptr<Node> m_root;
	std::vector<std::unique_ptr<CameraNode>> m_cam_nodes;
};

#endif
