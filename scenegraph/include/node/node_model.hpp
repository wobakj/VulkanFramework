#ifndef NODE_MODEL_HPP
#define NODE_MODEL_HPP

#include "node.hpp"
#include "ren/database_model.hpp"
#include "ren/database_transform.hpp"

class ModelNode : public Node
{
public:
  ModelNode();
  ModelNode(std::string const& name, std::string const& model, std::string const& transform);

  void accept(NodeVisitor &v) override;

 private:
  std::string m_model;
  std::string m_transform;

  friend class TransformVisitor;
  friend class PickVisitor;
  friend class BboxVisitor;
  friend class RenderVisitor;
  friend class Renderer;
};

#endif

