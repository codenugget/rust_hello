project(calc)

set(src
	Image.h
	Image_funcs.h
	Image_storage.h
	main.cpp
	stb_image.h
	stb_image_resize.h
	stb_image_write.h
	stb_impl.cpp
)

add_executable(${PROJECT_NAME} ${src})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
