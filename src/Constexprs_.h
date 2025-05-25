#pragma once

#include <vulkan/vulkan.hpp>
#include <imgui.h>

#ifdef RELEASE
constexpr bool USE_VALIDATION_LAYERS = false;
#else
constexpr bool USE_VALIDATION_LAYERS = true;
#endif

constexpr unsigned int EXPECTED_FRAME_RATE = 60;
constexpr unsigned int FRAME_OVERLAP = 2;
constexpr unsigned int NUMBER_OF_SWAPCHAIN_IMAGES = 3;
constexpr vk::SampleCountFlagBits MSAA_LEVEL = vk::SampleCountFlagBits::e2;

constexpr unsigned int ONE_MEBIBYTE_IN_BYTES = 1048576;
constexpr unsigned int ONE_SECOND_IN_MS = 1000;

constexpr unsigned int DEFAULT_VERTEX_BUFFER_SIZE = 100 * ONE_MEBIBYTE_IN_BYTES;
constexpr unsigned int DEFAULT_INDEX_BUFFER_SIZE = 100 * ONE_MEBIBYTE_IN_BYTES;

constexpr unsigned int MAX_IMAGE_SIZE = 100 * ONE_MEBIBYTE_IN_BYTES;
constexpr unsigned int MAX_INSTANCES = 1000;
constexpr unsigned int MAX_MATERIALS = 1000;

constexpr int NUMBER_OF_SKYBOX_VERTICES = 36;
constexpr int NUMBER_OF_CUBEMAP_FACES = 6;

constexpr ImColor IMGUI_BUTTON_RED = ImColor::ImColor(0.66f, 0.16f, 0.16f);
constexpr vk::ClearColorValue CLEAR_COLOR = vk::ClearColorValue(.463f, .616f, .859f, 0.f);