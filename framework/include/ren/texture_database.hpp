#ifndef TEXTURE_DATABASE_HPP
#define TEXTURE_DATABASE_HPP

#include "ren/database.hpp"
#include "wrap/memory.hpp"
#include "block_allocator.hpp"

#include <vulkan/vulkan.hpp>

#include <map>

class Device;
class Image;
class Transferrer;

class TextureDatabase : public Database<Image> {
 public:
  TextureDatabase();
  TextureDatabase(Transferrer& transferrer);
  TextureDatabase(TextureDatabase && dev);
  TextureDatabase(TextureDatabase const&) = delete;
  
  TextureDatabase& operator=(TextureDatabase const&) = delete;
  TextureDatabase& operator=(TextureDatabase&& dev);

  // void swap(TextureDatabase& dev);
  void store(std::string const& tex_path) override;
};

#endif