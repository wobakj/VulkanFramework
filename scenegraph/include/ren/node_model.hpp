#ifndef NODE_MODEL_HPP
#define NODE_MODEL_HPP

#include "ren/node_transform.hpp"

#include <string>

// class Device;
// class Transferrer;

class ModelNode : public TransformNode {
 public:  
  ModelNode();
  ModelNode(std::string const& model, std::string const& transform);
  ModelNode(ModelNode && dev);
  ModelNode(ModelNode const&) = delete;

  ModelNode& operator=(ModelNode const&) = delete;
  ModelNode& operator=(ModelNode&& dev);

  void swap(ModelNode& dev);

 private:
  std::string m_model;

  friend class Renderer;
};

#endif