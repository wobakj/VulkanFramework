#include "query_pool.hpp"

#include "device.hpp"

QueryPool::QueryPool()
 :WrapperQueryPool{}
 ,m_device{nullptr}
{}

QueryPool::QueryPool(QueryPool && rhs)
 :QueryPool{}
{
  swap(rhs);
}

QueryPool::QueryPool(Device const& device, vk::QueryType const& type, uint32_t count)
 :QueryPool{}
{
  m_device = &device;
  m_info.queryType = type;
  m_info.queryCount = count;
  m_object = device->createQueryPool(info());
}

QueryPool::~QueryPool() {
  cleanup();
}

void QueryPool::destroy() {
  (*m_device)->destroyQueryPool(get());
}

QueryPool& QueryPool::operator=(QueryPool&& dev) {
  swap(dev);
  return *this;
}

void QueryPool::swap(QueryPool& rhs) {
  WrapperQueryPool::swap(rhs);
  std::swap(m_device, rhs.m_device);
}

void QueryPool::timestamp(vk::CommandBuffer const& command_buffer, uint32_t index, vk::PipelineStageFlagBits const& stage) {
  command_buffer.writeTimestamp(stage, get(), index);
}

void QueryPool::reset(vk::CommandBuffer const& command_buffer) {
  command_buffer.resetQueryPool(get(), 0, info().queryCount);
}

void QueryPool::resetQuery(vk::CommandBuffer const& command_buffer, uint32_t index) {
  command_buffer.resetQueryPool(get(), index, 1);
}

void QueryPool::beginQuery(vk::CommandBuffer const& command_buffer, uint32_t index) {
  command_buffer.beginQuery(get(), index, vk::QueryControlFlags{});
}

void QueryPool::endQuery(vk::CommandBuffer const& command_buffer, uint32_t index) {
  command_buffer.endQuery(get(), index);
}

std::vector<bool> QueryPool::getAvaiabilities(uint32_t index, uint32_t num) const {
  std::vector<uint32_t> results = getResults<uint32_t>(index, num, vk::QueryResultFlagBits::eWithAvailability);
  std::vector<bool> avaiabilities(results.size());
  // avaiability stored after each query value
  for(std::size_t i = 0; i < results.size() / 2; ++i) {
    avaiabilities[i] = (results[i * 2 + 1] > 0) ? true : false;
  }
  return avaiabilities;
}
