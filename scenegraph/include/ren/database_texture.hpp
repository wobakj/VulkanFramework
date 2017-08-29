#ifndef DATABASE_TEXTURE_HPP
#define DATABASE_TEXTURE_HPP

#include "ren/database.hpp"
#include "wrap/image_res.hpp"
#include "wrap/sampler.hpp"
#include "deleter.hpp"

#include <vulkan/vulkan.hpp>

#include <map>

class Device;
class Transferrer;

class TextureDatabase : public Database<BackedImage> {
 public:
  TextureDatabase();
  TextureDatabase(Transferrer& transferrer);
  TextureDatabase(TextureDatabase && dev);
  TextureDatabase(TextureDatabase const&) = delete;
  
  TextureDatabase& operator=(TextureDatabase const&) = delete;
  TextureDatabase& operator=(TextureDatabase&& dev);

  void swap(TextureDatabase& dev);

  void store(std::string const& tex_path, BackedImage&& texture) override;
  void store(std::string const& tex_path);
  
  size_t index(std::string const& name) const;

  void writeToSet(vk::DescriptorSet& set, uint32_t binding) const;
  void writeToSet(vk::DescriptorSet& set, uint32_t first_binding, std::map<std::string, std::map<std::string, int32_t>> const& mapping) const;
 private:
  std::map<std::string, uint32_t> m_indices;
  Sampler m_sampler;
};

#endif