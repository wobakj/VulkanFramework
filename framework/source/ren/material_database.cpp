#include "ren/material_database.hpp"

#include "wrap/device.hpp"
#include "wrap/image.hpp"
#include "texture_loader.hpp"
#include "transferrer.hpp"


// material_t::material_t(tinyobj::material_t const& mat)
//  :vec_diffuse{mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]}
//  ,tex_diffuse{mat.diffuse_texname}
// {}

MaterialDatabase::MaterialDatabase()
 :Database{}
{}

MaterialDatabase::MaterialDatabase(MaterialDatabase && rhs)
{
  swap(rhs);
}

MaterialDatabase::MaterialDatabase(Transferrer& transferrer)
 :Database{transferrer}
{}

MaterialDatabase& MaterialDatabase::operator=(MaterialDatabase&& rhs) {
  swap(rhs);
  return *this;
}
