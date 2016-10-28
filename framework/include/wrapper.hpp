#ifndef WRAPER_HPP
#define WRAPER_HPP

#include <vulkan/vulkan.h>
#include <functional>

template <typename T>
class Wrapper {
public:
  Wrapper()
   :object{VK_NULL_HANDLE}
 {}

  Wrapper(T const& t) = delete;

  Wrapper(T&& t)
   :Wrapper{} 
  {
    swap(t);
  }

  virtual ~Wrapper() {
      cleanup();
  }

  const T* operator &() const {
      return &object;
  }

  T* replace() {
      cleanup();
      return &object;
  }

  void replace(T&& rhs) {
      swap(rhs);
  }
  void replace(T const& rhs) = delete;

  operator T() const {
      return object;
  }

  T const& get() const {
      return object;
  }

  T const* operator->() const {
    return &object;
  }

  T& operator=(T rhs) {
    swap(rhs);
    return *this;
  }

  void swap(T& rhs) {
    cleanup();
    object = rhs;
    rhs = VK_NULL_HANDLE;
  }

  // template<typename V>
  // bool operator==(V rhs) {
  //     return object == T(rhs);
  // }
  T& get() {
      return object;
  }

  T* operator->() {
    return &object;
  }
 protected:
  T* operator &() {
      return &object;
  }


  void set(T const& obj) {
    object = obj;
  }
  
  virtual void destroy() = 0;
 private:
  T object;

  void cleanup() {
      if (object) {
        destroy();
      }
      object = VK_NULL_HANDLE;
  }
};

#endif