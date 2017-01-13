#ifndef SEMAPHORE_HPP
#define SEMAPHORE_HPP

#include <mutex>
#include <condition_variable>
#include <stdexcept>

class Semaphore {
 public:
  Semaphore(uint32_t count = 0)
   :m_count(count) 
  {}

  inline void signal(uint32_t count = 1) {
    { 
      std::unique_lock<std::mutex> lock(m_mutex);
      m_count += count;
    }
    if (count > 1) {
      m_condition_lock.notify_one();
    }
    else {
      m_condition_lock.notify_all();
    }
  }

  inline void set(uint32_t count) {
    { 
      std::unique_lock<std::mutex> lock(m_mutex);
      m_count = count;
    }
    m_condition_lock.notify_all();
  }

  inline void shutDown() {
    { 
      std::unique_lock<std::mutex> lock(m_mutex);
      m_count = UINT32_MAX;
    }
    m_condition_lock.notify_all();
  }

  inline void unsignal() {
    { 
      std::unique_lock<std::mutex> lock(m_mutex);
      m_count = 0;
    }
    m_condition_lock.notify_one();
  }

  inline void wait() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condition_lock.wait(lock, [&]{return m_count > 0;});
    if (m_count > 0) {
      m_count--;
    }
    else {
      throw std::runtime_error("");
    }
  }

 private:
  std::mutex m_mutex;
  std::condition_variable m_condition_lock;
  uint32_t m_count;
};
#endif