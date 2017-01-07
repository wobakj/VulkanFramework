#ifndef QUERY_POOL_HPP
#define QUERY_POOL_HPP

#include "wrapper.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>

class Device;

// TODO: support pipeline statistics
using WrapperQueryPool = Wrapper<vk::QueryPool, vk::QueryPoolCreateInfo>;
class QueryPool : public WrapperQueryPool {
 public:
  
  QueryPool();
  QueryPool(Device const& device, vk::QueryType const& type = vk::QueryType::eTimestamp, uint32_t count = 1);
  QueryPool(QueryPool && rhs);
  QueryPool(QueryPool const&) = delete;
  ~QueryPool();

  QueryPool& operator=(QueryPool const&) = delete;
  QueryPool& operator=(QueryPool&& rhs);
// usage
  void reset(vk::CommandBuffer const& command_buffer);
  void resetQuery(vk::CommandBuffer const& command_buffer, uint32_t index);
  void beginQuery(vk::CommandBuffer const& command_buffer, uint32_t index);
  void endQuery(vk::CommandBuffer const& command_buffer, uint32_t index);
  void timestamp(vk::CommandBuffer const& command_buffer, uint32_t index, vk::PipelineStageFlagBits const& stage);
// results
  std::vector<bool> getAvaiabilities(uint32_t index, uint32_t num) const;
  // in ms
  std::vector<double> getTimes(bool wait = true) const;
  template<typename T>
  std::vector<T> getValues(bool wait = true) const;

  template<typename T>
  std::vector<T> getValues(uint32_t index, uint32_t num, bool wait = true) const;

  template<typename T>
  std::vector<T> getResults(uint32_t index, uint32_t num, vk::QueryResultFlags const flags) const;
  
  void swap(QueryPool& rhs);
  
 private:
  void destroy() override;

  Device const* m_device;
  float m_tick_duration;
};

#include "device.hpp"

template<typename T>
std::vector<T> QueryPool::getValues(bool wait) const {
  return getValues<T>(0, info().queryCount, wait);
}
// TODO: support for partial values?
template<typename T>
std::vector<T> QueryPool::getValues(uint32_t index, uint32_t num, bool wait) const {
  vk::QueryResultFlags flags{wait ? vk::QueryResultFlagBits::eWait : vk::QueryResultFlags{}};
  if (typeid(T) == typeid(uint64_t)) {
    flags |= vk::QueryResultFlagBits::e64;
  }
  return getResults<T>(index, num, flags);
}

template<typename T>
std::vector<T> QueryPool::getResults(uint32_t index, uint32_t num, vk::QueryResultFlags const flags) const {
  if (info().queryType == vk::QueryType::eTimestamp && flags & vk::QueryResultFlagBits::ePartial) {
    throw std::runtime_error{"Flag not compatible with type"};
  }

  if (flags & vk::QueryResultFlagBits::e64) {
    if (typeid(T) != typeid(uint64_t)) {
      throw std::runtime_error{"Incorrect return type specified"};
    }
  }
  else {
    if (typeid(T) != typeid(uint32_t)) {
      throw std::runtime_error{"Incorrect return type specified"};
    }
  }

  std::vector<T> results(num);
  if (flags & vk::QueryResultFlagBits::eWithAvailability) {
    results.resize(num * 2);
  }
  
  (*m_device)->getQueryPoolResults(get(), index, num, results.size() * sizeof(T), results.data(), sizeof(T), flags);
  return results;
}

#endif