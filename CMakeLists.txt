# CMakeList.txt : CMake project for exmview, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
# Add source to this project's executable.
add_executable (exmview WIN32
	"exmview.cpp"
	"widgets.cpp"
	"gui.cpp"
	#"lua-nanovg.c"
	"nanovg/src/nanovg.c"
)

target_compile_definitions(exmview PRIVATE "FONS_USE_FREETYPE=1")

# TODO: Add tests and install targets if needed.
#find_package(SDL2 CONFIG REQUIRED)
#target_link_libraries(exmview PRIVATE SDL2::SDL2 SDL2::SDL2main)

#target_include_directories(exmview PRIVATE tinyfx/khronos)

#find_package(lua53 CONFIG REQUIRED)
#target_link_libraries(exmview PRIVATE lua)


find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(exmview PRIVATE glfw)

find_package(gl3w CONFIG REQUIRED)
target_link_libraries(exmview PRIVATE unofficial::gl3w::gl3w)

#find_package(imgui CONFIG REQUIRED)
#target_link_libraries(exmview PRIVATE imgui::imgui)

find_package(Freetype REQUIRED)
target_link_libraries(exmview PRIVATE Freetype::Freetype)

#find_package(sdl2-ttf CONFIG REQUIRED)
#target_link_libraries(exmview PRIVATE SDL2::SDL2_ttf)

#find_package(glm CONFIG REQUIRED)
#target_link_libraries(exmview PRIVATE glm)
