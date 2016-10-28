#ifndef WRAPER_HPP
#define WRAPER_HPP

#include <vulkan/vulkan.h>
#include <functional>

template <typename T, typename U>
class Wrapper {
public:
  Wrapper()
   :m_object{VK_NULL_HANDLE}
   ,m_info{}
 {}

  explicit Wrapper(T const& t) = delete;

  explicit Wrapper(T&& t)
   :Wrapper{} 
  {
    swap(t);
  }

  virtual ~Wrapper() {
      cleanup();
  }

  const T* operator &() const {
      return &m_object;
  }

  T* replace() {
      cleanup();
      return &m_object;
  }

  void replace(T&& rhs) {
      swap(rhs);
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
  }
 private:
  T m_object;
  U m_info;

  void swap(T& rhs) {
    cleanup();
    m_object = rhs;
    rhs = VK_NULL_HANDLE;
  }

  void cleanup() {
      if (m_object) {
        destroy();
      }
      m_object = VK_NULL_HANDLE;
  }
};

#endif