#ifndef NODE_HPP
#define NODE_HPP

#include <string>

// class Device;
// class Transferrer;

class Node {
 public:  
  Node();
  Node(std::string const& model, std::string const& transform);
  Node(Node && dev);
  Node(Node const&) = delete;

  Node& operator=(Node const&) = delete;
  Node& operator=(Node&& dev);

  void swap(Node& dev);

 private:
  std::string m_model;
  std::string m_transform;

  friend class Renderer;
};

#endif