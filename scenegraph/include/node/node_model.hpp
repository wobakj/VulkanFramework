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

  Bbox const& getOrientedBox() const;
  std::shared_ptr<Hit> orientedBoxIntersectsRay(Ray const& r);

  void setOrientedBoxPoints(glm::vec3 const& min, glm::vec3 const& max);
  void setOrientedBoxTransform(glm::mat4 const& transform);

  void accept(NodeVisitor &v) override;

  std::string m_model;
  std::string m_transform;
 private:
  Bbox m_oriented_box;
};

#endif

