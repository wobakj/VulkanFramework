#ifndef NODELIGHT_HPP
#define NODELIGHT_HPP

#include <string>

#include "node.hpp"
#include "visit/visitor_node.hpp"

class LightNode : public Node
{
public:
	LightNode();
	LightNode(std::string const& name, std::string const& light);

	void accept(NodeVisitor &v) override;
  std::string const& id () const {
    return m_light;
  }
private:
  std::string m_light;
};

#endif

