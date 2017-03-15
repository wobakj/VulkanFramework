#ifndef NODE_TRANSFORM_HPP
#define NODE_TRANSFORM_HPP

#include <string>

// class Device;
// class Transferrer;

class TransformNode {
 public:  
  TransformNode();
  TransformNode(std::string const& transform);
  TransformNode(TransformNode && dev);
  TransformNode(TransformNode const&) = delete;
  ~TransformNode(){};

  TransformNode& operator=(TransformNode const&) = delete;
  TransformNode& operator=(TransformNode&& dev);

  void swap(TransformNode& dev);

 protected:
  std::string m_transform;

  friend class Renderer;
};

#endif