#ifndef TEXTURE_DATABASE_HPP
#define TEXTURE_DATABASE_HPP

#include "ren/database.hpp"
#include "wrap/image.hpp"
#include "deleter.hpp"

#include <vulkan/vulkan.hpp>

#include <map>

class Device;
class Transferrer;

class TextureDatabase : public Database<Image> {
 public:
  TextureDatabase();
  TextureDatabase(Transferrer& transferrer);
  TextureDatabase(TextureDatabase && dev);
  TextureDatabase(TextureDatabase const&) = delete;
  
  TextureDatabase& operator=(TextureDatabase const&) = delete;
  TextureDatabase& operator=(TextureDatabase&& dev);

  void swap(TextureDatabase& dev);

  void store(std::string const& tex_path, Image&& texture) override;
  void store(std::string const& tex_path);
  
  size_t index(std::string const& name) const;

  void writeToSet(vk::DescriptorSet& set, uint32_t binding) const;
 private:
  std::map<std::string, uint32_t> m_indices;
  Deleter<VkSampler> m_sampler;
};

#endif