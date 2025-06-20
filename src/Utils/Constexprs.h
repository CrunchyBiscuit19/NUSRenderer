#pragma once

#include <vulkan/vulkan.hpp>
#include <imgui.h>

#ifdef RELEASE
constexpr bool USE_VALIDATION_LAYERS = false;
#else
constexpr bool USE_VALIDATION_LAYERS = true;
#endif

constexpr bool LOG_TO_FILE = false;

constexpr unsigned int EXPECTED_FRAME_RATE = 60;
constexpr unsigned int FRAME_OVERLAP = 2;
constexpr unsigned int NUMBER_OF_SWAPCHAIN_IMAGES = 3;
constexpr auto MSAA_LEVEL = vk::SampleCountFlagBits::e8;

constexpr unsigned int ONE_MEBIBYTE_IN_BYTES = 1048576;
constexpr unsigned int ONE_SECOND_IN_MS = 1000;

constexpr unsigned int MESH_VERTEX_BUFFER_SIZE = 10 * ONE_MEBIBYTE_IN_BYTES;
constexpr unsigned int MESH_INDEX_BUFFER_SIZE = 10 * ONE_MEBIBYTE_IN_BYTES;

constexpr unsigned int MAIN_VERTEX_BUFFER_SIZE = 10 * MESH_VERTEX_BUFFER_SIZE;
constexpr unsigned int MAIN_INDEX_BUFFER_SIZE = 10 * MESH_INDEX_BUFFER_SIZE;

constexpr unsigned int MAX_IMAGE_SIZE = 100 * ONE_MEBIBYTE_IN_BYTES;
constexpr unsigned int MAX_INSTANCES = 1000;
constexpr unsigned int MAX_MATERIALS = 1000;
constexpr unsigned int MAX_NODES = 2000;
constexpr unsigned int MAX_RENDER_ITEMS = 2500;
constexpr unsigned int MAX_TEXTURE_ARRAY_SLOTS = 10000;

constexpr unsigned int MAX_CULL_LOCAL_SIZE = 256;

constexpr int NUMBER_OF_SKYBOX_VERTICES = 36;
constexpr int NUMBER_OF_CUBEMAP_FACES = 6;

constexpr auto IMGUI_HEADER_GREEN = ImColor(0.22f, 0.69f, 0.502f);
constexpr auto IMGUI_BUTTON_RED = ImColor(0.66f, 0.16f, 0.16f);
constexpr auto CLEAR_COLOR = vk::ClearColorValue(.463f, .616f, .859f, 0.f);
