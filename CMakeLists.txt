# CMakeList.txt : CMake project for NUSRenderer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 4.1.0-rc1)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Not sure what this is
#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")

# Change output directory of binaries
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

# Set shader target and grab all source files
file(GLOB_RECURSE SLANG_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/source/*.slang"
)
add_custom_target(Shaders ALL SOURCES ${SLANG_SOURCES})

# Set C++ Standard, and grab all source files
project(NUSRenderer)
file(GLOB_RECURSE CPP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Renderer/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Scene/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Data/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Utils/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/User/*.cpp"
)
file(GLOB_RECURSE HEADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Renderer/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Scene/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Data/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Utils/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/User/*.h"
)
add_executable(NUSRenderer ${CPP_SOURCES} ${HEADER_SOURCES})
set_property(TARGET NUSRenderer PROPERTY CXX_STANDARD 23)

# Vulkan
find_package(Vulkan REQUIRED)

# .dll libraries
add_subdirectory(thirdParty/fastgltf)
add_subdirectory(thirdParty/fmt)
add_subdirectory(thirdParty/SDL)

# Libraries with source code
set(CMAKE_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdParty")
add_library(imgui STATIC)
target_include_directories(imgui PUBLIC "${CMAKE_THIRD_PARTY_DIR}/imgui")
target_sources(imgui PRIVATE
  "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui.cpp"
  "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui_demo.cpp"
  "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui_draw.cpp"
  "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui_widgets.cpp"
  "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui_tables.cpp"
  "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui_impl_vulkan.cpp"
  "${CMAKE_THIRD_PARTY_DIR}/imgui/imgui_impl_sdl2.cpp"
  "${CMAKE_THIRD_PARTY_DIR}/imgui/ImGuizmo.cpp"
  #"${CMAKE_THIRD_PARTY_DIR}/imgui/GraphEditor.cpp"
  #"${CMAKE_THIRD_PARTY_DIR}/imgui/ImCurveEdit.cpp"
  #"${CMAKE_THIRD_PARTY_DIR}/imgui/ImGradient.cpp"
  #"${CMAKE_THIRD_PARTY_DIR}/imgui/ImSequencer.cpp"
)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL2::SDL2)
add_library(vkbootstrap STATIC)
target_include_directories(vkbootstrap PUBLIC "${CMAKE_THIRD_PARTY_DIR}/vkbootstrap")
target_sources(vkbootstrap PRIVATE 
  "${CMAKE_THIRD_PARTY_DIR}/vkbootstrap/VkBootstrap.h"
  "${CMAKE_THIRD_PARTY_DIR}/vkbootstrap/VkBootstrap.cpp"
)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)
set_property(TARGET vkbootstrap PROPERTY CXX_STANDARD 23)

# Libraries header-only
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE "${CMAKE_THIRD_PARTY_DIR}/glm")
add_library(magic_enum INTERFACE)
target_include_directories(magic_enum INTERFACE "${CMAKE_THIRD_PARTY_DIR}/magic_enum")
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE "${CMAKE_THIRD_PARTY_DIR}/stb_image")
add_library(imgui-filebrowser INTERFACE)
target_include_directories(imgui-filebrowser INTERFACE "${CMAKE_THIRD_PARTY_DIR}/imgui-filebrowser")
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE "${CMAKE_THIRD_PARTY_DIR}/vma")
add_library(quill INTERFACE)
target_include_directories(quill INTERFACE "${CMAKE_THIRD_PARTY_DIR}")

# Include directory + Link libraries + Shaders compilation
target_include_directories(NUSRenderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/")
target_link_libraries(NUSRenderer PUBLIC vkbootstrap imgui glm magic_enum stb_image imgui-filebrowser vma quill fmt Vulkan::Vulkan SDL2::SDL2 fastgltf::fastgltf)
add_dependencies(NUSRenderer Shaders)

# Compilation
target_precompile_headers(NUSRenderer PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <vulkan/vulkan.hpp> <vulkan/vulkan_raii.hpp>)
target_compile_definitions(NUSRenderer PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC LOGS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/logs/")
target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC MODELS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/models/")
target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC SKYBOXES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/skyboxes/")
set(CMAKE_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
file(MAKE_DIRECTORY "${CMAKE_SHADER_DIR}/out")
target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC SHADERS_PATH="${CMAKE_SHADER_DIR}/out")
target_compile_definitions(NUSRenderer PRIVATE $<$<CONFIG:Release>:RELEASE>)

# After compilation, copy SDL DLL
add_custom_command(TARGET NUSRenderer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:NUSRenderer> $<TARGET_FILE_DIR:NUSRenderer>
  COMMAND_EXPAND_LISTS
)