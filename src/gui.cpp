#include <gui.h>
#include <renderer.h>

#include <magic_enum.hpp>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <fmt/core.h>
#include <uuid.h>
#include <glm/gtc/type_ptr.hpp>

#include <ranges>

GUI::GUI(Renderer* renderer) :
	mRenderer(renderer)
{}

void GUI::init() {
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(mRenderer->mWindow);

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
    auto colorAttachmentFormat = vk::Format::eR16G16B16A16Sfloat;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    pipelineRenderingCreateInfo.depthAttachmentFormat = vk::Format::eD32Sfloat;

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = *mRenderer->mInstance;
    initInfo.PhysicalDevice = *mRenderer->mChosenGPU;
    initInfo.Device = *mRenderer->mDevice;
    initInfo.Queue = *mRenderer->mGraphicsQueue;
    initInfo.DescriptorPool = *mRenderer->mImmSubmit.mDescriptorPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.UseDynamicRendering = true;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;
    ImGui_ImplVulkan_Init(&initInfo);

    ImGui_ImplVulkan_CreateFontsTexture(); 
    ImGui_ImplVulkan_DestroyFontsTexture(); // ?

    mRenderer->mSelectModelFileDialog = ImGui::FileBrowser::FileBrowser(ImGuiFileBrowserFlags_::ImGuiFileBrowserFlags_MultipleSelection, RESOURCES_PATH);
    mRenderer->mSelectModelFileDialog.SetTitle("Select GLTF / GLB file");
    mRenderer->mSelectModelFileDialog.SetTypeFilters({ ".glb", ".gltf" });
}

void GUI::cleanup() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
}

void GUI::imguiFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    if (ImGui::Begin("Camera")) {
        ImGui::Text("[F1] Camera Mode: %s", magic_enum::enum_name(mRenderer->mCamera.movementMode).data());
        ImGui::Text("[F2] Mouse Mode: %s", (mRenderer->mCamera.relativeMode ? "RELATIVE" : "NORMAL"));
        ImGui::SliderFloat("Speed", &mRenderer->mCamera.speed, 0.f, 100.f, "%.2f");
        ImGui::Text("Position: %.1f, %.1f, %.1f", mRenderer->mCamera.position.x, mRenderer->mCamera.position.y, mRenderer->mCamera.position.z);
        ImGui::Text("Pitch: %.1f, Yaw: %.1f", mRenderer->mCamera.pitch, mRenderer->mCamera.yaw);
        if (ImGui::Button("Go to Origin")) {
            mRenderer->mCamera.position = glm::vec3();
        }
        ImGui::End();
    }
    if (ImGui::Begin("Stats")) {
        ImGui::Text("Compile Mode: %s", (bUseValidationLayers ? "DEBUG" : "RELEASE"));
        ImGui::Text("Frame Time:  %fms", mRenderer->mStats.mFrametime);
        ImGui::Text("Draw Time:  %fms", mRenderer->mStats.mDrawTime);
        ImGui::Text("Update Time: %fms", mRenderer->mStats.mSceneUpdateTime);
        ImGui::Text("Draws: %i", mRenderer->mStats.mDrawCallCount);
        ImGui::End();
    }
    if (ImGui::Begin("Models")) {
        if (ImGui::Button("Add Model")) {
            mRenderer->mSelectModelFileDialog.Open();
        }
        for (auto& model : mRenderer->mModels | std::views::values) {
            const auto name = model.mName;
            if (ImGui::TreeNode(name.c_str())) {
                if (ImGui::Button("Add Instance")) {
                    model.createInstance();
                }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::ImColor(0.66f, 0.16f, 0.16f)));
                if (ImGui::Button("Delete Model")) {
                    model.mToDelete = true;
                    mRenderer->mRegenRenderItems = true;
                }
                ImGui::PopStyleColor();

                for (auto& instance : model.mInstances) {
                    ImGui::SeparatorText(fmt::format("Instance {}-{}", model.mName, instance.mId).c_str());
                    ImGui::PushID(fmt::format("Instance {}-{}", model.mName, instance.mId).c_str());
                    ImGui::InputFloat3("Translation", glm::value_ptr(instance.mTransformComponents.translation));
                    ImGui::SliderFloat3("Pitch / Yaw / Roll", glm::value_ptr(instance.mTransformComponents.rotation), -glm::pi<float>(), glm::pi<float>());
                    ImGui::SliderFloat("Scale", &instance.mTransformComponents.scale, 0.f, 100.f);
                    ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::ImColor(0.66f, 0.16f, 0.16f)));
                    if (ImGui::Button("Delete Instance")) {
                        instance.mToDelete = true;
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                }

                ImGui::TreePop();
            }
            ImGui::Separator();
        }
        ImGui::End();

        mRenderer->mSelectModelFileDialog.Display();
        if (mRenderer->mSelectModelFileDialog.HasSelected()) {
            auto selectedFiles = mRenderer->mSelectModelFileDialog.GetMultiSelected();
            mRenderer->mSceneManager.loadModels(selectedFiles);
            mRenderer->mSelectModelFileDialog.ClearSelected();
            mRenderer->mRegenRenderItems = true;
        }
    }

    ImGui::Render();
}