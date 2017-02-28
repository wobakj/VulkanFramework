#include "wrap/fence.hpp"

#include "wrap/device.hpp"

Fence::Fence()
 :WrapperFence{}
 ,m_device{nullptr}
{}

Fence::Fence(Fence && Fence)
 :WrapperFence{}
{
  swap(Fence);
}

Fence::Fence(Device const& device, vk::FenceCreateFlags const& flags)
 :Fence{}
{
  m_device = &device;
  m_object = device->createFence({flags});
}

Fence::~Fence() {
  cleanup();
}

void Fence::destroy() {
  (*m_device)->destroyFence(get());
}

void Fence::wait() {
  if ((*m_device)->waitForFences({get()}, VK_TRUE, 100000000) != vk::Result::eSuccess) {
    assert(0);
    throw std::runtime_error{"waited too long for fence"};
  }
}

void Fence::reset() {
  (*m_device)->resetFences(get());
}

Fence& Fence::operator=(Fence&& Fence) {
  swap(Fence);
  return *this;
}

void Fence::swap(Fence& Fence) {
  WrapperFence::swap(Fence);
  std::swap(m_device, Fence.m_device);
}