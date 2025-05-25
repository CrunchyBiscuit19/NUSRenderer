#include <Gui.h>
#include <Renderer.h>

#include <magic_enum.hpp>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <fmt/core.h>
#include <glm/gtc/type_ptr.hpp>

#include <ranges>

GUI::GUI(Renderer* renderer) :
	mRenderer(renderer)
{
}

void GUI::init() {
	ImGui::CreateContext();
	ImGui_ImplSDL2_InitForVulkan(mRenderer->mRendererCore.mWindow);

	vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &mRenderer->mRendererInfrastructure.mSwapchainBundle.mFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = mRenderer->mRendererInfrastructure.mDepthImage.imageFormat;

	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = *mRenderer->mRendererCore.mInstance;
	initInfo.PhysicalDevice = *mRenderer->mRendererCore.mChosenGPU;
	initInfo.Device = *mRenderer->mRendererCore.mDevice;
	initInfo.Queue = *mRenderer->mRendererCore.mGraphicsQueue;
	initInfo.DescriptorPool = *mRenderer->mImmSubmit.mDescriptorPool;
	initInfo.MinImageCount = 3;
	initInfo.ImageCount = 3;
	initInfo.UseDynamicRendering = true;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.UseDynamicRendering = true;
	initInfo.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;
	ImGui_ImplVulkan_Init(&initInfo);

	ImGui_ImplVulkan_CreateFontsTexture();
	ImGui_ImplVulkan_DestroyFontsTexture();

	mSelectModelFileDialog = ImGui::FileBrowser::FileBrowser(ImGuiFileBrowserFlags_::ImGuiFileBrowserFlags_MultipleSelection, MODELS_PATH);
	mSelectModelFileDialog.SetTitle("Select GLTF / GLB File");
	mSelectModelFileDialog.SetTypeFilters({ ".glb", ".gltf" });

	mSelectSkyboxFileDialog = ImGui::FileBrowser::FileBrowser(ImGuiFileBrowserFlags_::ImGuiFileBrowserFlags_SelectDirectory, SKYBOXES_PATH);
	mSelectSkyboxFileDialog.SetTitle("Select Directory of Skybox Image");
}

void GUI::cleanup() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
}

void GUI::imguiFrame() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("DockSpace Demo", nullptr, windowFlags);

	ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
	ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

		ImGuiID dockMainId = dockspaceId;
		ImGuiID dockLeftId = ImGui::DockBuilderSplitNode(dockMainId, ImGuiDir_Left, 0.25f, nullptr, &dockMainId);

		ImGui::DockBuilderDockWindow("Camera", dockLeftId);
		ImGui::DockBuilderDockWindow("Models", dockLeftId);
		ImGui::DockBuilderDockWindow("Scene", dockLeftId);

		ImGui::DockBuilderFinish(dockspaceId);
	}

	ImGui::End();
	ImGui::PopStyleVar(2);

	ImGui::SetNextWindowDockID(dockspaceId, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x * 0.25f, viewport->WorkSize.y), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Camera")) {
		ImGui::Text("[F1] Camera Mode: %s", magic_enum::enum_name(mRenderer->mCamera.movementMode).data());
		ImGui::Text("[F2] Mouse Mode: %s", (mRenderer->mCamera.relativeMode ? "RELATIVE" : "NORMAL"));
		ImGui::Text("Position: [%.1f, %.1f, %.1f], Pitch / Yaw: [%.1f, %.1f]", mRenderer->mCamera.position.x, mRenderer->mCamera.position.y, mRenderer->mCamera.position.z, mRenderer->mCamera.pitch, mRenderer->mCamera.yaw);
		ImGui::SliderFloat("Speed", &mRenderer->mCamera.speed, 0.f, 100.f, "%.2f");
		if (ImGui::Button("Go to Origin")) {
			mRenderer->mCamera.position = glm::vec3();
		}
		ImGui::End();
	}

	if (ImGui::Begin("Models")) {
		if (ImGui::Button("Add Model")) {
			mSelectModelFileDialog.Open();
		}
		for (auto& model : mRenderer->mRendererScene.mModels | std::views::values) {
			const auto name = model.mName;
			if (ImGui::TreeNode(name.c_str())) {
				if (ImGui::Button("Add Instance")) {
					model.createInstance();
				}
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(IMGUI_BUTTON_RED));
				if (ImGui::Button("Delete Model")) {
					for (auto& instance : model.mInstances) {
						instance.mDeleteSignal = true;
					}
					model.markDelete();
					mRenderer->mRegenRenderItems = true;
				}
				ImGui::PopStyleColor();

				for (auto& instance : model.mInstances) {
					ImGui::SeparatorText(fmt::format("{}-{}", model.mName, instance.mId).c_str());
					
					ImGui::PushID(fmt::format("{}-{}", model.mName, instance.mId).c_str());
					
					if (ImGui::InputFloat3("Translation", glm::value_ptr(instance.mTransformComponents.translation))) { model.mReloadInstancesBuffer = true; };
					if (ImGui::SliderFloat3("Pitch / Yaw / Roll", glm::value_ptr(instance.mTransformComponents.rotation), -glm::pi<float>(), glm::pi<float>())) { model.mReloadInstancesBuffer = true; }
					if (ImGui::SliderFloat("Scale", &instance.mTransformComponents.scale, 0.f, 100.f)) { model.mReloadInstancesBuffer = true; }
					
					ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(IMGUI_BUTTON_RED));
					if (ImGui::Button("Delete Instance")) { 
						instance.mDeleteSignal = true; 
						model.mReloadInstancesBuffer = true; 
					}
					ImGui::PopStyleColor();
					
					ImGui::PopID();
				}

				ImGui::TreePop();
			}
			ImGui::Separator();
		}
		ImGui::End();

		mSelectModelFileDialog.Display();
		if (mSelectModelFileDialog.HasSelected()) {
			auto selectedFiles = mSelectModelFileDialog.GetMultiSelected();
			mRenderer->mRendererScene.loadModels(selectedFiles);
			mSelectModelFileDialog.ClearSelected();
			mRenderer->mRegenRenderItems = true;
		}
	}

	if (ImGui::Begin("Scene")) {
		ImGui::ColorEdit3("Ambient Color", glm::value_ptr(mRenderer->mRendererScene.mSceneResources.mSceneData.ambientColor));
		ImGui::ColorEdit3("Sunlight Color", glm::value_ptr(mRenderer->mRendererScene.mSceneResources.mSceneData.sunlightColor));
		ImGui::InputFloat3("Sunlight Direction", glm::value_ptr(mRenderer->mRendererScene.mSceneResources.mSceneData.sunlightDirection));
		ImGui::InputFloat("Sunlight Power", &mRenderer->mRendererScene.mSceneResources.mSceneData.sunlightDirection[3]);
		if (ImGui::Button("Change Skybox")) {
			mSelectSkyboxFileDialog.Open();
		}
		if (ImGui::Button("Toggle Skybox")) {
			mRenderer->mRendererScene.mSkyboxActive = !mRenderer->mRendererScene.mSkyboxActive;
		}
		ImGui::End();

		mSelectSkyboxFileDialog.Display();
		if (mSelectSkyboxFileDialog.HasSelected()) {
			std::filesystem::path selectedSkyboxDir = mSelectSkyboxFileDialog.GetSelected();
			mRenderer->mRendererScene.mSkybox.updateSkyboxImage(selectedSkyboxDir);	
			mSelectSkyboxFileDialog.ClearSelected();
		}
	}

	if (ImGui::Begin("Stats")) {
		ImGui::Text("Compile Mode: %s", (USE_VALIDATION_LAYERS ? "DEBUG" : "RELEASE"));
		ImGui::Text("Frame Time:  %fms", mRenderer->mStats.mFrametime);
		ImGui::Text("Draw Time:  %fms", mRenderer->mStats.mDrawTime);
		ImGui::Text("Update Time: %fms", mRenderer->mStats.mSceneUpdateTime);
		ImGui::Text("Draws: %i", mRenderer->mStats.mDrawCallCount);
		ImGui::End();
	}
	ImGui::Render();
}