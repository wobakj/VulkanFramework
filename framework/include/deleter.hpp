#ifndef DELETER_HPP
#define DELETER_HPP

#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <functional>
#include <wrap/wrapper.hpp>
#include <wrap/device.hpp>

template <typename T>
class Deleter {
public:
    Deleter() : Deleter([](T, VkAllocationCallbacks*) {}) {}

    Deleter(std::function<void(T, VkAllocationCallbacks*)> deletef) {
        this->deleter = [=](T obj) { deletef(obj, nullptr); };
    }

    Deleter(const Deleter<VkInstance>& instance, std::function<void(VkInstance, T, VkAllocationCallbacks*)> deletef) {
        this->deleter = [&instance, deletef](T obj) { deletef(instance, obj, nullptr); };
    }

    Deleter(Device const& device, std::function<void(vk::Device const, T, VkAllocationCallbacks*)> deletef) {
        this->deleter = [&device, deletef](T obj) { deletef(device, obj, nullptr); };
    }

    Deleter(const Deleter<VkDevice>& device, std::function<void(VkDevice, T, VkAllocationCallbacks*)> deletef) {
        this->deleter = [&device, deletef](T obj) {deletef(device, obj, nullptr); };
    }

    template<typename U, typename V>
    Deleter(Wrapper<U, V> const& base, std::function<void(U const&, T, VkAllocationCallbacks*)> deletef) {
        this->deleter = [&base, deletef](T obj) {deletef(base.get(), obj, nullptr); };
    }

    // Deleter(Wrapper<VkDevice> const& device, std::function<void(VkDevice const, T, VkAllocationCallbacks*)> deletef) {
    //     this->deleter = [&device, deletef](T obj) { deletef(device, obj, nullptr); };
    // }
    Deleter(const vk::Instance& instance, std::function<void(VkInstance, T, VkAllocationCallbacks*)> deletef) {
        this->deleter = [&instance, deletef](T obj) { deletef(instance, obj, nullptr); };
    }

    // Deleter(const vk::Device device, std::function<void(VkDevice const, T, VkAllocationCallbacks*)> deletef) {
    //     this->deleter = [device, deletef](T obj) { 
    //         std::cout << "deleting from device "<< std::endl; deletef(device, obj, nullptr); };
    //     std::cout << "delete with " << & device << std::endl;
    // }

    ~Deleter() {
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

    void operator=(T rhs) {
        cleanup();
        object = rhs;
    }

    template<typename V>
    bool operator==(V rhs) {
        return object == T(rhs);
    }

private:
    T object{VK_NULL_HANDLE};
    std::function<void(T)> deleter;

    void cleanup() {
        if (object != VK_NULL_HANDLE) {
            deleter(object);
        }
        object = VK_NULL_HANDLE;
    }
};

#endif