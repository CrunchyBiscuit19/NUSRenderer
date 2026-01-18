#pragma once

#include <Utils/Types.h>
#include <Data/Mesh.h>
#include <Scene/Batch.h>

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
constexpr u32 MINOR_VERSION = 4;
constexpr u32 PATCH_VERSION = 0;

constexpr u32 MAX_DESCRIPTOR_SETS_PER_POOL = 1 << 12;

constexpr float MAX_CAMERA_SPEED = 10.f;

constexpr u32 EXPECTED_FRAME_RATE = 60;
constexpr u32 FRAME_OVERLAP = 2;
constexpr u32 NUMBER_OF_SWAPCHAIN_IMAGES = 3;

constexpr auto MSAA_ENABLE = false;
constexpr auto MSAA_LEVEL = vk::SampleCountFlagBits::e1;

constexpr u32 KIBIBYTE = 1 << 10;
constexpr u32 MEBIBYTE = 1 << 20;
constexpr u32 ONE_SECOND_IN_MS = 1e3;

constexpr u32 INITIAL_MESH_VERTEX_BUFFER_SIZE = 1e0 * MEBIBYTE;
constexpr u32 INITIAL_MESH_INDEX_BUFFER_SIZE = 1e0 * MEBIBYTE;
constexpr u32 INITIAL_MAIN_VERTEX_BUFFER_SIZE = 1e3 * INITIAL_MESH_VERTEX_BUFFER_SIZE;
constexpr u32 INITIAL_MAIN_INDEX_BUFFER_SIZE = 1e3 * INITIAL_MESH_INDEX_BUFFER_SIZE;
constexpr u32 INITIAL_MODEL_INSTANCES = 1e2;
constexpr u32 INITIAL_MAIN_INSTANCES = 1e4;
constexpr u32 INITIAL_MODEL_MATERIALS = 1e2;
constexpr u32 INITIAL_MAIN_MATERIALS = 1e4;
constexpr u32 INITIAL_MODEL_NODES = 1e2;
constexpr u32 INITIAL_MAIN_NODES = 1e4;
constexpr u32 INITIAL_MAIN_BOUNDS = 1e4;
constexpr u32 INITIAL_MODEL_BOUNDS = 1e6;
constexpr u32 DRAW_MAX_RENDER_ITEMS = 1e6;
constexpr u32 MAIN_MAX_RENDER_ITEMS = 1e6;
constexpr u32 BATCH_MAX_RENDER_ITEMS = 1e5;
constexpr u32 BATCH_MAX_RENDER_INSTANCES = 1e6;
constexpr u32 MAX_TEXTURE_ARRAY_SLOTS = 1e4;

constexpr u32 IMAGE_STAGING_BUFFER_SIZE = (1 << 10) * MEBIBYTE;
constexpr u32 MESH_STAGING_BUFFER_SIZE = (1 << 6) * MEBIBYTE;
constexpr u32 MATERIAL_CONSTANTS_STAGING_BUFFER_SIZE = (1 << 6) * INITIAL_MODEL_MATERIALS * sizeof(MaterialConstants);
constexpr u32 NODE_TRANSFORMS_STAGING_BUFFER_SIZE = (1 << 6) * INITIAL_MODEL_NODES * sizeof(glm::mat4);
constexpr u32 BOUNDS_STAGING_BUFFER_SIZE = (1 << 6) * INITIAL_MODEL_BOUNDS * sizeof(AABB);
constexpr u32 RENDER_ITEMS_STAGING_BUFFER_SIZE = (1 << 6) * BATCH_MAX_RENDER_ITEMS * sizeof(RenderItem);
constexpr u32 RENDER_INSTANCES_STAGING_BUFFER_SIZE = (1 << 6) * BATCH_MAX_RENDER_INSTANCES * sizeof(RenderInstance);

constexpr u32 NUMBER_OF_SKYBOX_VERTICES = 36;
constexpr u32 NUMBER_OF_CUBEMAP_FACES = 6;

constexpr float IMGUIZMO_SIZE = 0.15f;
constexpr auto IMGUI_HEADER_GREEN = ImColor(0.22f, 0.69f, 0.502f);
constexpr auto IMGUI_BUTTON_RED = ImColor(0.66f, 0.16f, 0.16f);
constexpr auto CLEAR_COLOR = vk::ClearColorValue(.463f, .616f, .859f, 0.f);

constexpr float FOVY = 70.f;
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 10000.f;

constexpr u32 MAX_1D_WORKGROUP_THREADS = 1024;
constexpr u32 MAX_2D_WORKGROUP_THREADS = 32;

constexpr u32 FRUSTUM_NUM_PLANES = 6;
constexpr u32 FRUSTUM_NEAR_FACE = 0;
constexpr u32 FRUSTUM_FAR_FACE = 1;
constexpr u32 FRUSTUM_LEFT_FACE = 2;
constexpr u32 FRUSTUM_RIGHT_FACE = 3;
constexpr u32 FRUSTUM_TOP_FACE = 4;
constexpr u32 FRUSTUM_BOTTOM_FACE = 5;
constexpr u32 MAX_DEPTH_PYRAMID_LEVELS = 16;

constexpr u32 NUM_FULLSCREEN_QUAD_VERTICES = 3;
