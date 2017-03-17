#ifndef NODELIGHT_HPP
#define NODELIGHT_HPP

#include <string>

#include "node.hpp"
#include "visit/visitor_node.hpp"

class LightNode : public Node
{
public:
	LightNode();
	LightNode(std::string &name, glm::mat4 const& transform, glm::vec4 &color, float const& brightness);

	void accept(NodeVisitor &v) override;

	glm::vec4 const& getColor() const;

private:
	glm::vec4 m_color;
	float m_brightness;

};

#endif

