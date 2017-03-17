#ifndef VISITORBBOX_HPP
#define VISITORBBOX_HPP

#include <memory>

#include "visit/visitor_node.hpp"
#include "bbox.hpp"
#include "node/node_model.hpp"
#include "node/node_light.hpp"
#include "node/node_screen.hpp"
#include "node/node_camera.hpp"
#include "frustum.hpp"

class BboxVisitor : public NodeVisitor
{
public:
	BboxVisitor();
	~BboxVisitor();

	Bbox const& getBox() const;

	void visit(Node* node) override;
	void visit(ModelNode* node) override;
	void visit(CameraNode* node) override;
	void visit(LightNode* node) override;
	void visit(ScreenNode* node) override;

private:
	Bbox m_box;
};

#endif