#include "ren/node.hpp"

// #include "wrap/device.hpp"
// #include "transferrer.hpp"

// #include <iostream>

Node::Node()
{}

Node::Node(Node && rhs)
 :Node{}
{
  swap(rhs);
}

Node::Node(std::string const& model, std::string const& transform)
 :m_model(model)
 ,m_transform{transform}
{}

Node& Node::operator=(Node&& rhs) {
  swap(rhs);
  return *this;
}

void Node::swap(Node& rhs) {
  std::swap(m_model, rhs.m_model);
  std::swap(m_transform, rhs.m_transform);
}