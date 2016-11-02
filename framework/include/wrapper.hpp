#ifndef WRAPPER_HPP
#define WRAPPER_HPP

#include <vulkan/vulkan.h>
#include <functional>

template <typename T, typename U>
class Wrapper {
public:
  Wrapper()
   :m_object{VK_NULL_HANDLE}
   ,m_info{}
   ,m_owner{false}
 {}

  explicit Wrapper(T const& t)
   :m_object{t}
   ,m_info{}
   ,m_owner{false}
  {}

  explicit Wrapper(T&& t)
   :Wrapper{} 
  {
    swapObject(t);
  }

  virtual ~Wrapper() {
    cleanup();
  }

  // const T* operator &() const {
  //   return &m_object;
  // }

  T* replace() {
    cleanup();
    return &m_object;
  }

  void replace(T&& rhs) {
    swapObject(rhs);
  }
  
  void replace(T const& rhs) = delete;

  operator T() const {
    return m_object;
  }

  T const& get() const {
    return m_object;
  }

  T const* operator->() const {
    return &m_object;
  }

  T& operator=(T rhs) {
    swap(rhs);
    return *this;
  }

  // template<typename V>
  // bool operator==(V rhs) {
  //     return m_object == T(rhs);
  // }
  T& get() {
    return m_object;
  }

  T* operator->() {
    return &m_object;
  }
  U const& info() const {
    return m_info;
  }

 protected:
  T* operator &() {
    return &m_object;
  }
  U& info() {
    return m_info;
  }

  virtual void destroy() = 0;

  void swap(Wrapper& rhs) {
    std::swap(m_object, rhs.m_object);
    std::swap(m_info, rhs.m_info);
    std::swap(m_owner, rhs.m_owner);
  }

 private:
  T m_object;
  U m_info;
  bool m_owner;

  void swapObject(T& rhs) {
    cleanup();
    m_object = rhs;
    rhs = VK_NULL_HANDLE;
    m_owner = true;
  }

  void cleanup() {
    if (m_owner) { 
      if (m_object) {
        destroy();
      }
      m_object = VK_NULL_HANDLE;
    }
  }
};

#endif