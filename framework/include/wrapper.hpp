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

  Wrapper(T& t)
   :object{t} 
  {}

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

  operator T() const {
      return object;
  }

  T const& get() const {
      return object;
  }

  T const* operator->() const {
    return &object;
  }

  void operator=(T rhs) {
      cleanup();
      object = rhs;
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