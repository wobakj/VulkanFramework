#ifndef NODEGEOMETRY_HPP
#define NODEGEOMETRY_HPP

#include "node.hpp"
#include "ren/database_model.hpp"
#include "ren/database_transform.hpp"

class GeometryNode : public Node
{
public:
  GeometryNode();
  GeometryNode(std::string const& name, std::string const& model, std::string const& transform);

  void accept(NodeVisitor &v) override;

  std::string m_model;
  std::string m_transform;
 private:
};

#endif

