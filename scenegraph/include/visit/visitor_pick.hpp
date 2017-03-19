#ifndef VISITORPICK_HPP
#define VISITORPICK_HPP

#include "visit/visitor_node.hpp"
#include "ray.hpp"
#include <vector>
#include <memory>


class Node;
class GeometryNode;
class CameraNode;
class LightNode;
class ScreenNode;
class Hit;
class Ray;
class ApplicationInstance;

class PickVisitor : public NodeVisitor
{
public:
	PickVisitor(ApplicationInstance& instance);

	void setRay(Ray const& r);
	std::vector<Hit> const& getHits() const;

	void visit(Node* node) override;
	void visit(ModelNode* node) override;
	void visit(CameraNode* node) override;
	void visit(LightNode* node) override;
	void visit(ScreenNode* node) override;

private:
	std::vector<Hit> m_hits;
	Ray m_ray;
	ApplicationInstance* m_instance;
};

#endif 

