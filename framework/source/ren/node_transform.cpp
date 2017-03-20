#include "ren/node_transform.hpp"

// #include "wrap/device.hpp"
// #include "transferrer.hpp"

// #include <iostream>

TransformNode::TransformNode()
{}

TransformNode::TransformNode(TransformNode && rhs)
 :TransformNode{}
{
  swap(rhs);
}

TransformNode::TransformNode(std::string const& transform)
 :m_transform{transform}
{}

TransformNode& TransformNode::operator=(TransformNode&& rhs) {
  swap(rhs);
  return *this;
}

void TransformNode::swap(TransformNode& rhs) {
  std::swap(m_transform, rhs.m_transform);
}