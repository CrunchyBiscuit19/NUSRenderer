#include <gui.h>
#include <renderer.h>

#include <magic_enum.hpp>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <fmt/core.h>
#include <uuid.h>

#include <ranges>

GUI::GUI(Renderer* renderer) :
	mRenderer(renderer)
{}

void GUI::init() {
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(mRenderer->mWindow);

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = *mRenderer->mInstance;
    initInfo.PhysicalDevice = *mRenderer->mChosenGPU;
    initInfo.Device = *mRenderer->mDevice;
    initInfo.Queue = *mRenderer->mGraphicsQueue;
    initInfo.DescriptorPool = *mRenderer->mImmSubmit.mDescriptorPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.UseDynamicRendering = true;
    initInfo.ColorAttachmentFormat = static_cast<VkFormat>(mRenderer->mSwapchainImageFormat);
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&initInfo, VK_NULL_HANDLE);

    mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) { ImGui_ImplVulkan_CreateFontsTexture(*cmd); });
    ImGui_ImplVulkan_DestroyFontUploadObjects();

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
    ImGui_ImplSDL2_NewFrame(mRenderer->mWindow);
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
        ImGui::Text("Pipeline binds: %i", mRenderer->mStats.mPipelineBinds);
        ImGui::Text("Layout binds: %i", mRenderer->mStats.mLayoutBinds);
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
                    GLTFInstance newInstance;
                    newInstance.id = model.mLatestId;
                    model.mLatestId++;
                    
                    if (!model.mLoaded) {
                        model.load();
                        model.mLoaded = true;
                    }
					
                    model.mInstances.push_back(newInstance);
                    mRenderer->mFlags.updateInstances = true;
                }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::ImColor(0.66f, 0.16f, 0.16f)));
                if (ImGui::Button("Delete Model")) {
                    model.mToDelete = true;
                    mRenderer->mFlags.updateModels = true;
                }
                ImGui::PopStyleColor();

                for (auto& instance : model.mInstances) {
                    ImGui::SeparatorText(fmt::format("Instance {}-{}", model.mName, instance.id).c_str());
                    ImGui::PushID(fmt::format("Instance {}-{}", model.mName, instance.id).c_str());
                    if (ImGui::InputFloat3("Translation", &instance.transformComponents.translation[0])) {
                        mRenderer->mFlags.updateInstances = true;
                    }
                    if (ImGui::SliderFloat3("Pitch / Yaw / Roll", &instance.transformComponents.rotation[0], -glm::pi<float>(), glm::pi<float>())) {
                        mRenderer->mFlags.updateInstances = true;
                    }
                    if (ImGui::SliderFloat("Scale", &instance.transformComponents.scale, 0.f, 100.f)) {
                        mRenderer->mFlags.updateInstances = true;
                    }
                    ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(ImColor::ImColor(0.66f, 0.16f, 0.16f)));
                    if (ImGui::Button("Delete Instance")) {
                        instance.toDelete = true;
                        mRenderer->mFlags.updateInstances = true;
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
            mRenderer->mFlags.updateModels = true;
        }
    }

    ImGui::Render();
}