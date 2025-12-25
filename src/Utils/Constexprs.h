#pragma once

#include <vulkan/vulkan.hpp>
#include <imgui.h>

#ifdef RELEASE
constexpr bool USE_VALIDATION_LAYERS = false;
#else
constexpr bool USE_VALIDATION_LAYERS = true;
#endif

constexpr bool LOG_TO_FILE = false;

constexpr uint32_t MAJOR_VERSION = 1;
constexpr uint32_t MINOR_VERSION = 3;
constexpr uint32_t PATCH_VERSION = 0;

constexpr float MAX_CAMERA_SPEED = 100.f;

constexpr float IMGUIZMO_SIZE = 0.15f;

constexpr unsigned int EXPECTED_FRAME_RATE = 60;
constexpr unsigned int FRAME_OVERLAP = 2;
constexpr unsigned int NUMBER_OF_SWAPCHAIN_IMAGES = 3;
constexpr auto MSAA_LEVEL = vk::SampleCountFlagBits::e8;

constexpr unsigned long ONE_MEBIBYTE_IN_BYTES = 1 << 20;
constexpr unsigned long ONE_SECOND_IN_MS = 1e3;

constexpr unsigned long MESH_VERTEX_BUFFER_SIZE = 1e3 * ONE_MEBIBYTE_IN_BYTES;
constexpr unsigned long MESH_INDEX_BUFFER_SIZE = 1e3 * ONE_MEBIBYTE_IN_BYTES;

constexpr unsigned long MAIN_VERTEX_BUFFER_SIZE = 1e3 * MESH_VERTEX_BUFFER_SIZE;
constexpr unsigned long MAIN_INDEX_BUFFER_SIZE = 1e3 * MESH_INDEX_BUFFER_SIZE;

constexpr unsigned long MAX_IMAGE_SIZE = 1e4 * ONE_MEBIBYTE_IN_BYTES;
constexpr unsigned int MAX_INSTANCES = 1e5;
constexpr unsigned int MAX_MATERIALS = 1e5;
constexpr unsigned int MAX_MESHES = 1e5;
constexpr unsigned int MAX_NODES = 1e5;
constexpr unsigned int MAX_RENDER_ITEMS = 1e5;
constexpr unsigned int MAX_TEXTURE_ARRAY_SLOTS = 1e6;

constexpr unsigned int MAX_CULL_LOCAL_SIZE = 256;

constexpr int NUMBER_OF_SKYBOX_VERTICES = 36;
constexpr int NUMBER_OF_CUBEMAP_FACES = 6;

constexpr auto IMGUI_HEADER_GREEN = ImColor(0.22f, 0.69f, 0.502f);
constexpr auto IMGUI_BUTTON_RED = ImColor(0.66f, 0.16f, 0.16f);
constexpr auto CLEAR_COLOR = vk::ClearColorValue(.463f, .616f, .859f, 0.f);

constexpr float FOVY = 70.f;
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 10000.f;

constexpr int FRUSTUM_NEAR_FACE = 0;
constexpr int FRUSTUM_FAR_FACE = 1;
constexpr int FRUSTUM_LEFT_FACE = 2;
constexpr int FRUSTUM_RIGHT_FACE = 3;
constexpr int FRUSTUM_TOP_FACE = 4;
constexpr int FRUSTUM_BOTTOM_FACE = 5;

