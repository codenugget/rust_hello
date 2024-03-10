#include <chrono>
#include <iostream>
#include <string>

#include <immintrin.h>

#include "Image.h"
#include "Image_storage.h"

void add_pixels_single_threaded_intrinsics(ImageLib::Image* img, const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a) {
	__m128i intr_x;
	__m128i intr_sum;
	const uint32_t n_tot = img->count_pixel_components();
	int32_t intr_n = n_tot >> 4;
	uint8_t* cur_ptr = img->vec_data().data();
	uint8_t inc[16] = { r, g, b, a, r, g, b, a, r, g, b, a, r, g, b, a };
	const __m128i intr_inc = _mm_load_si128((const __m128i*) inc);
	while (intr_n-- > 0) {
		intr_x = _mm_load_si128((const __m128i*) cur_ptr);
		intr_sum = _mm_adds_epu8(intr_x, intr_inc);
		_mm_store_si128((__m128i*) cur_ptr, intr_sum);
		cur_ptr += 16;
	}

	const uint32_t intr_rem = n_tot & 0xF;
	if (intr_rem > 0) {
		intr_x = _mm_loadu_si128((const __m128i*) cur_ptr);
		intr_sum = _mm_adds_epu8(intr_x, intr_inc);
		_mm_storeu_si128((__m128i*) cur_ptr, intr_sum);
		cur_ptr += intr_rem;
	}
}

int main(int argc, char** argv)
{
	std::string filename = "pillars.png";
	std::string filename_out = "output.png";
	auto orig = ImageLib::load(filename);
	if (!orig) {
		std::cerr << "Unable to load file " << filename << std::endl;
		exit(1);
	}
	if (orig->components() != 4) {
		std::cerr << "This program only supports rgba mode" << std::endl;
		exit(1);
	}

	auto img = orig->clone();
	const uint8_t increment = 32;

	const auto T0 = std::chrono::high_resolution_clock::now();
	add_pixels_single_threaded_intrinsics(img.get(), increment, increment, increment, 0);
	const auto dT0 = std::chrono::high_resolution_clock::now() - T0;
	std::cout << "Pixel conversion: " << std::chrono::duration_cast<std::chrono::microseconds>(dT0).count() << "us" << std::endl;

	ImageLib::save(img.get(), filename_out);
	exit(0);
}
