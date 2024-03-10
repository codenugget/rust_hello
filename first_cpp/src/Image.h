#pragma once

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace ImageLib
{
  template<typename T>
  class TImage
  {
  public:
    TImage(const int width, const int height, const int num_components) :
      m_width(width), m_height(height), m_num_components(num_components),
      m_pixel_data(width * height * num_components, T(0)) {
    }

    int width() const { return m_width; }
    int height() const { return m_height; }
    int components() const { return m_num_components; }

    int count_pixels() const { return width() * height(); }
    int count_pixel_components() const { return count_pixels() * components(); }
    int count_bytes() const { return static_cast<int>(m_pixel_data.size() * sizeof(T)); }
    int stride_byte_size() const { return width() * components() * sizeof(T); }

    void set_flipped(bool f) { m_image_flipped = f ? 1 : 0; }
    bool is_flipped() const { return m_image_flipped != 0; }

    bool is_same_shape(const TImage& other) const {
      return width() == other.width() && height() == other.height()
        && components() == other.components()
        && is_flipped() == other.is_flipped();
    }

    // This is not super fast but convenient
    void set(const int x, const int y, const int c, const T v) {
      if (x < 0 || x >= m_width || y < 0 || y >= m_height || c < 0 || c >= m_num_components)
        return;
      m_pixel_data[(y * m_width + x) * m_num_components + c] = v;
    }

    T get(const int x, const int y, const int c) const {
      if (x < 0 || x >= m_width || y < 0 || y >= m_height || c < 0 || c >= m_num_components)
        return T(0);
      return m_pixel_data[(y * m_width + x) * m_num_components + c];
    }

    std::unique_ptr<TImage<T>> clone() const {
      std::unique_ptr<TImage<T>> ret = std::make_unique<TImage<T>>(m_width, m_height, m_num_components);
      memcpy(ret->data(), data(), count_bytes());
      return ret;
    }

    const T* data() const { return m_pixel_data.data(); }
    T* data() { return m_pixel_data.data(); }

    const std::vector<T>& vec_data() const { return m_pixel_data; }
    std::vector<T>& vec_data() { return m_pixel_data; }

  private:
    std::vector<T> m_pixel_data;
    int m_width = 0;
    int m_height = 0;
    int m_num_components = 3;
    int m_image_flipped = 0;
  };

  typedef TImage<float> fImage;
  typedef TImage<double> dImage;
  typedef TImage<unsigned char> Image;
} // namespace
