#pragma once

#include <memory>
#include <string>
#include <tuple>

#include "stb_image.h"
#include "stb_image_write.h"

#include "Image.h"
#include "Image_funcs.h"

namespace ImageLib
{
  inline std::unique_ptr<Image> load(const std::string& filename, bool flip_vertical = false) {
    int w, h, nc;
    unsigned char* data = stbi_load(filename.c_str(), &w, &h, &nc, 0);
    if (!data)
      return nullptr;

    if (flip_vertical)
      flip_vertically(w, h, nc, data);

    auto image = std::make_unique<Image>(w, h, nc);
    image->set_flipped(flip_vertical);
    memcpy(image->data(), data, image->count_pixel_components());
    stbi_image_free(data);
    return std::move(image);
  }

  // returns true + empty if saved, false + reason if failed
  inline std::tuple<bool, std::string> save(const Image* img, const std::string& filename) {
    if (img == nullptr)
      return std::make_tuple(false, "image parameter is nullptr");

    const Image* i = img;
    std::unique_ptr<Image> tmp;
    if (img->is_flipped())
    {
      tmp = img->clone();
      flip_vertically(img->width(), img->height(), img->components(), tmp->data());
      i = tmp.get();
    }
    auto pos = filename.rfind('.');  //TODO: handle exeption if there is no dot
    std::string ext = filename.substr(pos);
    for (auto& c : ext)
      c = static_cast<char>(::tolower(c));
    if (ext == ".png")
    {
      if (stbi_write_png(filename.c_str(), i->width(), i->height(), i->components(), i->data(), i->stride_byte_size()) == 1)
        return std::make_tuple(true, "");
      return std::make_tuple(false, "Unable to save as png");
    }
    else if (ext == ".bmp")
    {
      if (stbi_write_bmp(filename.c_str(), i->width(), i->height(), i->components(), i->data()) == 1)
        return std::make_tuple(true, "");
      return std::make_tuple(false, "Unable to save as bmp");
    }
    else if (ext == ".tga")
    {
      if (stbi_write_tga(filename.c_str(), i->width(), i->height(), i->components(), i->data()) == 1)
        return std::make_tuple(true, "");
      return std::make_tuple(false, "Unable to save as tga");
    }
    else if (ext == ".jpg")
    {
      if (stbi_write_jpg(filename.c_str(), i->width(), i->height(), i->components(), i->data(), 100) == 1)
        return std::make_tuple(true, "");
      return std::make_tuple(false, "Unable to save as jpg");
    }
    return std::make_tuple(false, "Unsupported file type \"" + ext + "\"");
  }

} // namespace
