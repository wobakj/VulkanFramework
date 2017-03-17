#ifndef NODE_HPP
#define NODE_HPP

#include "nodeVisitor.hpp"
#include "bbox.hpp"
#include "ray.hpp"

#include <string>
#include <glm\mat4x4.hpp>
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
	~Node();

	std::unique_ptr<Node> copy() const;

	void setName(std::string const &name);
	void setWorld(glm::mat4 const &world);
	void setLocal(glm::mat4 const &local);
	void setBox(std::shared_ptr<Bbox> const &box);
	void setParent(std::shared_ptr<Node> const &p);

	std::string getName() const;
	glm::mat4 getWorld() const;
	glm::mat4 getLocal() const;
	std::shared_ptr<Bbox> getBox() const;
	std::shared_ptr<Node> getParent() const;
	std::shared_ptr<Scenegraph> getScenegraph() const;

	std::vector<std::shared_ptr<Node>> getChildren();
	bool hasChildren();
	void addChild(std::shared_ptr<Node> n);
	void removeChild(std::shared_ptr<Node> const& child);
	void clearChildren();

	void scale(glm::vec3 const& s);
	void rotate(float angle, glm::vec3 const& r);
	void translate(glm::vec3 const& t);

	//virtual std::set<hit> ray_test();
	std::shared_ptr<hit> intersectsRay(Ray const& r) const;
	virtual void accept(NodeVisitor &v) = 0;

protected:
	std::string m_name;
	glm::mat4 m_world;
	glm::mat4 m_local;
	std::shared_ptr<bbox> m_box;
	std::vector<std::shared_ptr<Node>> m_children;

	std::shared_ptr<Node> m_parent;
	std::shared_ptr<Scenegraph> m_scenegraph;
};

#endif
