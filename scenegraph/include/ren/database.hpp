#ifndef DATABASE_HPP
#define DATABASE_HPP

#include "wrap/memory.hpp"
#include "allocator_block.hpp"

#include <vulkan/vulkan.hpp>

#include <map>

class Device;
class Image;
class Transferrer;

template<typename T>
class Database {
 public:
  Database();
  Database(Transferrer& transferrer);
  Database(Database && dev);
  Database(Database const&) = delete;
  
  Database& operator=(Database const&) = delete;
  Database& operator=(Database&& dev);

  virtual void swap(Database& dev);

  virtual T const& get(std::string const& tex_path);
  
  void set(std::string const& tex_path, T const& resource);
  void set(std::string const& tex_path, T&& resource);
  // virtual void store(std::string const& path);
  virtual void store(std::string const& name, T&& resource);
  bool contains(std::string const& tex_path);
  size_t size() const;
 protected:
  Device const* m_device;
  Transferrer* m_transferrer;
  BlockAllocator m_allocator;

  std::map<std::string, T> m_resources;

  const static size_t SIZE_RESOURCE;
};

#include "transferrer.hpp"

template<typename T>
const size_t Database<T>::SIZE_RESOURCE = sizeof(T);

template<typename T>
Database<T>::Database()
 :m_device{nullptr}
 ,m_transferrer{nullptr}
{}

template<typename T>
Database<T>::Database(Database && rhs)
{
  swap(rhs);
}

template<typename T>
Database<T>::Database(Transferrer& transferrer)
 :Database{}
{
  m_transferrer = &transferrer;
  m_device = &transferrer.device();
}

template<typename T>
Database<T>& Database<T>::operator=(Database&& rhs) {
  swap(rhs);
  return *this;
}

template<typename T>
void Database<T>::swap(Database& rhs) {
  std::swap(m_device, rhs.m_device);
  std::swap(m_allocator, rhs.m_allocator);
  std::swap(m_transferrer, rhs.m_transferrer);
  std::swap(m_resources, rhs.m_resources);
}

template<typename T>
T const& Database<T>::get(std::string const& tex_path) {
  return m_resources.at(tex_path);
}

template<typename T>
bool Database<T>::contains(std::string const& tex_path) {
  return m_resources.find(tex_path) != m_resources.end();
}

template<typename T>
void Database<T>::store(std::string const& name, T&& resource) {
  if (m_resources.find(name) != m_resources.end()) {
    throw std::runtime_error{"key already in use"};
  }
  m_resources.emplace(name, std::move(resource));  
}

template<typename T>
void Database<T>::set(std::string const& name, T const& resource) {
  m_resources.at(name) = resource;
}

template<typename T>
void Database<T>::set(std::string const& name, T&& resource) {
  m_resources.at(name) = std::move(resource);
}

template<typename T>
size_t Database<T>::size() const {
  return m_resources.size();  
}

// template<typename T>
// void Database<T>::store(std::string const& path) {
//   throw std::exception{};
// }
#endif