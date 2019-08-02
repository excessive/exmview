# CMakeList.txt : CMake project for exmview, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

if (WIN32)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

# Add source to this project's executable.
add_executable (exmview WIN32
	"exmview.cpp"
	"widgets.cpp"
	"gui.cpp"
	#"lua-nanovg.c"
	"nanovg/src/nanovg.c"
)

target_compile_definitions(exmview PRIVATE "FONS_USE_FREETYPE=1")

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(exmview PRIVATE glfw)

find_package(gl3w CONFIG REQUIRED)
target_link_libraries(exmview PRIVATE unofficial::gl3w::gl3w)

find_package(Freetype REQUIRED)
target_link_libraries(exmview PRIVATE Freetype::Freetype)
