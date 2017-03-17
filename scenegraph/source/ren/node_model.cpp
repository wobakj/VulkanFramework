#include "ren/node_model.hpp"

// #include "wrap/device.hpp"
// #include "transferrer.hpp"

// #include <iostream>

ModelNode::ModelNode()\
 :TransformNode{}
{}

ModelNode::ModelNode(ModelNode && rhs)
 :ModelNode{}
{
  swap(rhs);
}

ModelNode::ModelNode(std::string const& model, std::string const& transform)
 :TransformNode{transform}
 ,m_model(model)
{}

ModelNode& ModelNode::operator=(ModelNode&& rhs) {
  swap(rhs);
  return *this;
}

void ModelNode::swap(ModelNode& rhs) {
  TransformNode::swap(rhs);
  std::swap(m_model, rhs.m_model);
}