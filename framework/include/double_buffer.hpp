#ifndef DOUBLE_BUFFER_HPP
#define DOUBLE_BUFFER_HPP

#include <algorithm>
#include <functional>

template<typename T>
struct DoubleBuffer{
  DoubleBuffer()
   :m_front{}
   ,m_back{}
   ,dirty{false}
  {}
  
  DoubleBuffer(T& f, T& b)
   :m_front{f}
   ,m_back{b}
   ,dirty{false}
  {}

  T const& get() const{
    if(dirty) {
      swap();
    }
    return front;
  }    

  T& front() {
    return m_front;
  }

  T& back() {
    return m_back;
  }
  // allow swapping of references
  T m_front;
  T m_back;
  bool dirty;

  void swap(){
    std::swap(m_front, m_back);
    dirty = false;
  }
};

#endif