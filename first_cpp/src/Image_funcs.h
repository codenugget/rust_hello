#pragma once

#include <cstring>
#include <memory>

#include "Image.h"

namespace ImageLib
{

  template <typename T>
  T rgb_to_gray(const T r, const T g, const T b)
  {
    return static_cast<T>(0.2126 * r + 0.7152 * g + 0.0722 * b);
  }

  template <typename T>
  T rgb_to_gray(const unsigned char r, const unsigned char g, const unsigned char b)
  {
    return rgb_to_gray<T>(static_cast<T>(r / 255.0), static_cast<T>(g / 255.0), static_cast<T>(b / 255.0));
  }

  template <typename T>
  void rgb_to_ycbcr(const T r, const T g, const T b, T& Y, T& Cb, T& Cr)
  {
    Y  = static_cast<T>(        0.299000 * r + 0.587000 * g + 0.114000 * b);
    Cb = static_cast<T>(128.0 - 0.168736 * r - 0.331264 * g + 0.500000 * b);
    Cr = static_cast<T>(128.0 + 0.500000 * r - 0.418688 * g - 0.081312 * b);
  }

  template <typename T>
  void rgb_to_ycbcr(const T rgb[3], T YCbCr[3])
  {
    rgb_to_ycbcr(rgb[0], rgb[1], rgb[2], YCbCr[0], YCbCr[1], YCbCr[2]);
  }

  template <typename T>
  void ycbcr_to_rgb(const T Y, const T Cb, const T Cr, T& r, T& g, T& b)
  {
    r = static_cast<T>(Y                           + 1.402000 * (Cr - 128.0));
    g = static_cast<T>(Y - 0.344136 * (Cb - 128.0) - 0.714136 * (Cr - 128.0));
    b = static_cast<T>(Y + 1.772000 * (Cb - 128.0)                          );
  }

  template <typename T>
  void ycbcr_to_rgb(const T ycbcr[3], T rgb[3])
  {
    ycbcr_to_rgb(ycbcr[0], ycbcr[0], ycbcr[0], rgb[0], rgb[1], rgb[2]);
  }

  inline void flip_vertically(const int w, const int h, const int nc, unsigned char* data)
  {
    unsigned char t;
    int row_bytes = w * nc;
    int num_j = (int)h >> 1;
    for (int j = 0; j < num_j; ++j) {
      unsigned char* p1 = data + j * row_bytes;
      unsigned char* p2 = data + (h - 1 - j) * row_bytes;
      for (int i = 0; i < (int)row_bytes; ++i) {
        t = p1[i], p1[i] = p2[i], p2[i] = t;
      }
    }
  }

  template<typename T>
  std::unique_ptr<TImage<T>> create_filled_image(int w, int h, int c, const T fill_value)
  {
    auto img = std::make_unique<TImage<T>>(w, h, c);
    int n = img->count_pixel_components();
    int nc = img->components();
    T* v = img->data();
    for(int i = 0; i < n; ++i)
      v[i] = fill_value;
    return std::move(img);
  }

  template<typename T>
  std::unique_ptr<TImage<T>> values_like(const TImage<T>* in, const T fill_value)
  {
    return create_filled_image<T>(in->width(), in->height(), in->components(), fill_value);
  }

  template<typename T>
  std::unique_ptr<TImage<T>> zeros_like(const TImage<T>* in) { return values_like<T>(in, T(0)); }
  template<typename T>
  std::unique_ptr<TImage<T>> ones_like(const TImage<T>* in) { return values_like<T>(in, T(1)); }

  template<typename T>
  bool copyto(TImage<T>* dst, const TImage<T>* src) {
    if (!dst or !src or !dst->is_same_shape(*src))
      return false;
    memcpy(dst->data(), src->data(), src->count_bytes());
    return true;
  }


  inline std::unique_ptr<TImage<double>> convert_rgb_to_ycbcr_dbl(const uint32_t w, const uint32_t h, const uint32_t c, const uint8_t* data)
  {
    std::unique_ptr<TImage<double>> ret = std::make_unique<TImage<double>>(w, h, 3);
    double* dst = ret->data();

    uint8_t ur, ug, ub;
    double r, g, b, y, cb, cr;
    for (uint32_t row = 0; row < h; ++row)
    {
      for (uint32_t col = 0; col < w; ++col)
      {
        ur = data[row * w * c + col * c + 0];
        ug = ur; ub = ur;
        if (c > 2) {
          ug = data[row * w * c + col * c + 1];
          ub = data[row * w * c + col * c + 2];
        }
        r = ur; g = ug; b = ub;
        rgb_to_ycbcr(r, g, b, y, cb, cr);
        dst[row * w * 3 + col * 3 + 0] = y;
        dst[row * w * 3 + col * 3 + 1] = cb;
        dst[row * w * 3 + col * 3 + 2] = cr;
      }
    }
    return ret;
  }

  inline std::unique_ptr<Image> convert_rgb_to_ycbcr(const uint32_t w, const uint32_t h, const uint32_t c, const uint8_t* data) {
    std::unique_ptr<Image> ret = std::make_unique<Image>(w, h, 3);
    unsigned char* dst = ret->data();

    auto to_u8 = [](double v) -> unsigned char {
      if (v < 0)
        return 0;
      if (v > 255)
        return 255;
      else
        return static_cast<unsigned char>(v + 0.5);
    };

    uint8_t ur, ug, ub;
    double r, g, b, y, cb, cr;
    for (uint32_t row = 0; row < h; ++row)
    {
      for (uint32_t col = 0; col < w; ++col)
      {
        ur = data[row * w * c + col * c + 0];
        ug = ur; ub = ur;
        if (c > 2) {
          ug = data[row * w * c + col * c + 1];
          ub = data[row * w * c + col * c + 2];
        }
        r = ur; g = ug; b = ub;
        rgb_to_ycbcr(r, g, b, y, cb, cr);
        ur = to_u8(y);
        ug = to_u8(cb);
        ub = to_u8(cr);

        dst[row * w * 3 + col * 3 + 0] = ur;
        dst[row * w * 3 + col * 3 + 1] = ug;
        dst[row * w * 3 + col * 3 + 2] = ub;
      }
    }
    return ret;
  }

  template<typename T>
  std::unique_ptr<TImage<T>> extract_component(const uint32_t ec, const uint32_t w, const uint32_t h, const uint32_t c, const T* data)
  {
    if (ec >= c)
      return nullptr;
    std::unique_ptr<TImage<T>> ret = std::make_unique<TImage<T>>(w, h, 1);
    T* dst = ret->data();
    for (uint32_t row = 0; row < h; ++row)
      for (uint32_t col = 0; col < w; ++col)
        dst[row * w + col] = data[row * w * c + col * c + ec];
    return ret;
  }
} // namespace
