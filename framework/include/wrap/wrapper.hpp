#ifndef WRAPPER_HPP
#define WRAPPER_HPP

#include <functional>

template <typename T, typename U>
class Wrapper {
public:

  operator T const&() const {
    return m_object;
  }
  operator bool() const {
    return m_object;
  }

  // call methods on underlying object
  T const* operator->() const {
    return &m_object;
  }

  U const& info() const {
    return m_info;
  }

  // public, to call in case the conversion operator fails
  T const& get() const {
    return m_object;
  }

 protected:
  // allow construction only through derived class
  Wrapper()
   :m_object{}
   ,m_info{}
  {}

  explicit Wrapper(T&& t, U const& info)
   :Wrapper{} 
  {
    swapObjects(std::move(t), info);
  }

  void replace(T&& rhs, U const& info) {
    swapObjects(std::move(rhs), info);
  }
  // prevent accidental replace without ownership transfer
  void replace(T const& rhs) = delete;

  void swap(Wrapper& rhs) {
    std::swap(m_object, rhs.m_object);
    std::swap(m_info, rhs.m_info);
  }
  // must be called in destructor of derived class
  void cleanup() {
    if (m_object) {
      destroy();
    }
    m_object = T{};
  }
  // freeing of resources, implemented in derived class
  virtual void destroy() = 0;

  T m_object;
  U m_info;

 private:
  void swapObjects(T&& rhs, U const& info) {
    cleanup();
    std::swap(m_object, rhs);
    m_info = info;
  }
};

#endif