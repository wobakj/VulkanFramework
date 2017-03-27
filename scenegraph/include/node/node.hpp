#ifndef NODE_HPP
#define NODE_HPP

#include "visit/visitor_node.hpp"
#include "bbox.hpp"

#include <string>
#include <glm/mat4x4.hpp>
#include <vector>
#include <set>
#include <memory>

class NodeVisitor;
class Scenegraph;

class Node
{
public:
	Node();
	Node(std::string const&name, glm::mat4 const& local  = glm::fmat4{1.0f});

	void setLocal(glm::mat4 const &local);

	std::string const& getName() const;
	glm::mat4 const& getWorld() const;
	glm::mat4 const& getLocal() const;
	Bbox getBox() const;
	Node* getParent() const;

	std::vector<Node*> getChildren();
	Node* getChild(std::string const& name);
	bool hasChildren();
	void addChild(std::unique_ptr<Node>&& n);
	std::unique_ptr<Node> detachChild(std::string const& name);
	void clearChildren();

	void scale(glm::vec3 const& s);
	void rotate(float angle, glm::vec3 const& r);
	void translate(glm::vec3 const& t);
	
 protected:
	virtual void accept(NodeVisitor &v);
	glm::fmat4 m_local;
 
 private:	
	void setWorld(glm::mat4 const &world);
	std::vector<std::unique_ptr<Node>>::iterator findChild(std::string const& name);
	void setBox(Bbox const& box);
	void setParent(Node* const p);

	Node* m_parent;
	std::string m_name;
	glm::fmat4 m_world;
	Bbox m_box;
	std::vector<std::unique_ptr<Node>> m_children;

	friend class TransformVisitor;
	friend class PickVisitor;
	friend class BboxVisitor;
	friend class RenderVisitor;
};

#endif
