#ifndef NODE_HPP
#define NODE_HPP

#include "visit/visitor_node.hpp"
#include "bbox.hpp"
#include "ray.hpp"

#include <string>
#include <glm/mat4x4.hpp>
#include <vector>
#include <set>
#include <memory>

class Hit;
class NodeVisitor;
class Scenegraph;

class Node
{
public:
	Node();
	Node(std::string const &name, glm::mat4 const);

	void setName(std::string const &name);
	void setWorld(glm::mat4 const &world);
	void setLocal(glm::mat4 const &local);
	void setBox(Bbox const& box);

	std::string getName() const;
	glm::mat4 getWorld() const;
	glm::mat4 getLocal() const;
	Bbox getBox() const;
	Node* getParent() const;
	Scenegraph* getScenegraph() const;

	std::vector<Node*> getChildren();
	bool hasChildren();
	void addChild(std::unique_ptr<Node>&& n);
	void removeChild(std::unique_ptr<Node> const child);
	void clearChildren();

	void scale(glm::vec3 const& s);
	void rotate(float angle, glm::vec3 const& r);
	void translate(glm::vec3 const& t);

	std::shared_ptr<Hit> intersectsRay(Ray const& r) const;
	virtual void accept(NodeVisitor &v);

protected:
	void setParent(Node* const p);
	
	Node* m_parent;
	std::string m_name;
	glm::mat4 m_world;
	glm::mat4 m_local;
	Bbox m_box;
	std::vector<std::unique_ptr<Node>> m_children;

	Scenegraph * m_scenegraph;

	friend class NodeVisitor;
};

#endif
