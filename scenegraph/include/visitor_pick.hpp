#ifndef VISITORPICK_HPP
#define VISITORPICK_HPP

#include "visitor_node.hpp"

class Node;
class GeometryNode;
class CameraNode;
class LightNode;
class ScreenNode;
class Hit;
class Ray;

class PickVisitor : public NodeVisitor
{
public:
	PickVisitor();
	~PickVisitor();

	void setRay(ray const& r);

	void visit(Node* node) override;
	void visit(GeometryNode* node) override;
	void visit(CameraNode* node) override;
	void visit(LightNode* node) override;
	void visit(ScreenNode* node) override;

private:
	std::set<std::shared_ptr<Hit>> m_hits;
	Ray m_ray;
};

#endif 

