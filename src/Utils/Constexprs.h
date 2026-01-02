#pragma once

#include <Utils/Types.h>
#include <imgui.h>

#include <vulkan/vulkan.hpp>

enum class ValidationMode { None, Basic, Strict };

enum class LogLocation { Console, File, Both };

#ifdef RELEASE
constexpr ValidationMode VALIDATION_MODE = ValidationMode::None;
#else
constexpr ValidationMode VALIDATION_MODE = ValidationMode::Basic;
#endif

constexpr LogLocation LOG_LOCATION = LogLocation::Both;

constexpr u32 MAJOR_VERSION = 1;
constexpr u32 MINOR_VERSION = 3;
constexpr u32 PATCH_VERSION = 0;

constexpr u32 MAX_DESCRIPTOR_SETS_PER_POOL = 1 << 12;

constexpr float MAX_CAMERA_SPEED = 100.f;

constexpr u32 EXPECTED_FRAME_RATE = 60;
constexpr u32 FRAME_OVERLAP = 2;
constexpr u32 NUMBER_OF_SWAPCHAIN_IMAGES = 3;
constexpr auto MSAA_LEVEL = vk::SampleCountFlagBits::e8;

constexpr u32 ONE_MEBIBYTE_IN_BYTES = 1 << 20;
constexpr u32 ONE_SECOND_IN_MS = 1e3;

constexpr u32 MESH_VERTEX_BUFFER_SIZE = 1e3 * ONE_MEBIBYTE_IN_BYTES;
constexpr u32 MESH_INDEX_BUFFER_SIZE = 1e3 * ONE_MEBIBYTE_IN_BYTES;
constexpr u32 MAIN_VERTEX_BUFFER_SIZE = 1e3 * MESH_VERTEX_BUFFER_SIZE;
constexpr u32 MAIN_INDEX_BUFFER_SIZE = 1e3 * MESH_INDEX_BUFFER_SIZE;
constexpr u32 MAX_IMAGE_SIZE = 1e4 * ONE_MEBIBYTE_IN_BYTES;
constexpr u32 MAX_INSTANCES = 1e5;
constexpr u32 MAX_MATERIALS = 1e5;
constexpr u32 MAX_MESHES = 1e5;
constexpr u32 MAX_NODES = 1e5;
constexpr u32 MAX_RENDER_ITEMS = 1e5;
constexpr u32 MAX_TEXTURE_ARRAY_SLOTS = 1e6;

constexpr u32 NUMBER_OF_SKYBOX_VERTICES = 36;
constexpr u32 NUMBER_OF_CUBEMAP_FACES = 6;

constexpr float IMGUIZMO_SIZE = 0.15f;
constexpr auto IMGUI_HEADER_GREEN = ImColor(0.22f, 0.69f, 0.502f);
constexpr auto IMGUI_BUTTON_RED = ImColor(0.66f, 0.16f, 0.16f);
constexpr auto CLEAR_COLOR = vk::ClearColorValue(.463f, .616f, .859f, 0.f);

constexpr float FOVY = 70.f;
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 10000.f;

constexpr u32 MAX_CULL_LOCAL_SIZE = 256;
constexpr u32 FRUSTUM_NUM_PLANES = 6;
constexpr u32 FRUSTUM_NEAR_FACE = 0;
constexpr u32 FRUSTUM_FAR_FACE = 1;
constexpr u32 FRUSTUM_LEFT_FACE = 2;
constexpr u32 FRUSTUM_RIGHT_FACE = 3;
constexpr u32 FRUSTUM_TOP_FACE = 4;
constexpr u32 FRUSTUM_BOTTOM_FACE = 5;
constexpr u32 MAX_DEPTH_PYRAMID_MIPS = 16;
