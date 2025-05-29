#include <User/Gui.h>
#include <Renderer/Renderer.h>

#include <magic_enum.hpp>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <fmt/core.h>
#include <glm/gtc/type_ptr.hpp>

#include <ranges>

GuiComponent::GuiComponent(Renderer* renderer, Gui* gui, std::string name) :
	mRenderer(renderer),
	mGui(gui),
	mName(name)
{
}

void Gui::CameraGuiComponent::elements()
{
	ImGui::Text("Camera Mode: %s", magic_enum::enum_name(mRenderer->mCamera.mMovementMode).data());
	ImGui::Text("Mouse Mode: %s", (mRenderer->mCamera.mRelativeMode ? "RELATIVE" : "NORMAL"));
	ImGui::Text("Position: [%.1f, %.1f, %.1f]", mRenderer->mCamera.mPosition.x, mRenderer->mCamera.mPosition.y, mRenderer->mCamera.mPosition.z);
	ImGui::Text("Pitch / Yaw: [%.1f, %.1f]", mRenderer->mCamera.mPitch, mRenderer->mCamera.mYaw);
	ImGui::SliderFloat("Speed", &mRenderer->mCamera.mSpeed, 0.f, 100.f, "%.2f");
}

void Gui::SceneGuiComponent::elements()
{
	if (ImGui::CollapsingHeader("Models")) {
		if (ImGui::Button("Add Model")) {
			mGui->mSelectModelFileDialog.Open();
		}
		for (auto& model : mRenderer->mRendererScene.mModels | std::views::values) {
			const auto name = model.mName;
			ImGui::PushStyleColor(ImGuiCol_Header, static_cast<ImVec4>(IMGUI_HEADER_GREEN));
			if (ImGui::CollapsingHeader(name.c_str())) {
				if (ImGui::Button(fmt::format("Add Instance##{}", name).c_str())) {
					model.createInstance();
				}
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(IMGUI_BUTTON_RED));
				if (ImGui::Button(fmt::format("Delete Model##{}", name).c_str())) {
					for (auto& instance : model.mInstances) {
						instance.mDeleteSignal = true;
					}
					model.markDelete();
					mRenderer->mRegenRenderItems = true;
				}
				ImGui::PopStyleColor();

				for (auto& instance : model.mInstances) {
					if (ImGui::TreeNode(fmt::format("{}-{}", model.mName, instance.mId).c_str())) {
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
						ImGui::TreePop();
					}
				}
			}
			ImGui::PopStyleColor();
		}
	}
	if (ImGui::CollapsingHeader("Sunlight")) {
		ImGui::ColorEdit3("Ambient Color", glm::value_ptr(mRenderer->mRendererScene.mSceneResources.mSceneData.ambientColor));
		ImGui::ColorEdit3("Sunlight Color", glm::value_ptr(mRenderer->mRendererScene.mSceneResources.mSceneData.sunlightColor));
		ImGui::SliderFloat3("Sunlight Direction", glm::value_ptr(mRenderer->mRendererScene.mSceneResources.mSceneData.sunlightDirection), 0.f, 1.f);
		ImGui::InputFloat("Sunlight Power", &mRenderer->mRendererScene.mSceneResources.mSceneData.sunlightDirection[3]);
	}
	if (ImGui::CollapsingHeader("Skybox")) {
		if (ImGui::Button("Change Skybox")) {
			mGui->mSelectSkyboxFileDialog.Open();
		}
		ImGui::SameLine();
		if (ImGui::Button("Toggle Skybox")) {
			mRenderer->mRendererScene.mSkyboxActive = !mRenderer->mRendererScene.mSkyboxActive;
		}
	}

	mGui->mSelectSkyboxFileDialog.Display();
	if (mGui->mSelectSkyboxFileDialog.HasSelected()) {
		std::filesystem::path selectedSkyboxDir = mGui->mSelectSkyboxFileDialog.GetSelected();
		mRenderer->mRendererScene.mSkybox.updateSkyboxImage(selectedSkyboxDir);
		mGui->mSelectSkyboxFileDialog.ClearSelected();
	}

	mGui->mSelectModelFileDialog.Display();
	if (mGui->mSelectModelFileDialog.HasSelected()) {
		auto selectedFiles = mGui->mSelectModelFileDialog.GetMultiSelected();
		mRenderer->mRendererScene.loadModels(selectedFiles);
		mGui->mSelectModelFileDialog.ClearSelected();
		mRenderer->mRegenRenderItems = true;
	}
}

void Gui::MiscGuiComponent::elements()
{
	if (ImGui::CollapsingHeader("Stats")) {
		ImGui::Text("VALIDATION LAYERS: %s", (USE_VALIDATION_LAYERS ? "ON" : "OFF"));
		ImGui::Text("Frame Time:  %fms", mRenderer->mStats.mFrametime);
		ImGui::Text("Draw Time:  %fms", mRenderer->mStats.mDrawTime);
		ImGui::Text("Update Time: %fms", mRenderer->mStats.mSceneUpdateTime);
		ImGui::Text("Draws: %i", mRenderer->mStats.mDrawCallCount);
	}
	if (ImGui::CollapsingHeader("Hotkeys")) {
		ImGui::Text("[F12] Change Camera Mode");
		ImGui::Text("[F11] Change Mouse Mode");
		ImGui::Text("[F10] Toggle Borderless Fullscreen");
	}
}

Gui::Gui(Renderer* renderer) :
	mRenderer(renderer),
	mDescriptorPool(nullptr)
{
}

void Gui::init() {
	std::vector<vk::DescriptorPoolSize> poolSizes = {
		{ vk::DescriptorType::eSampler, 100 },
		{ vk::DescriptorType::eCombinedImageSampler, 100 },
		{ vk::DescriptorType::eSampledImage, 100 },
		{ vk::DescriptorType::eStorageImage, 100 },
		{ vk::DescriptorType::eUniformTexelBuffer, 100 },
		{ vk::DescriptorType::eStorageTexelBuffer, 100 },
		{ vk::DescriptorType::eUniformBuffer, 100 },
		{ vk::DescriptorType::eUniformBufferDynamic, 100 },
		{ vk::DescriptorType::eStorageBuffer, 100 },
		{ vk::DescriptorType::eStorageBufferDynamic, 100 },
		{ vk::DescriptorType::eInputAttachment, 100 },
	};
	vk::DescriptorPoolCreateInfo poolInfo = {};
	poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	poolInfo.maxSets = 100;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	mDescriptorPool = mRenderer->mRendererCore.mDevice.createDescriptorPool(poolInfo);

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
	initInfo.DescriptorPool = *mDescriptorPool;
	initInfo.MinImageCount = NUMBER_OF_SWAPCHAIN_IMAGES;
	initInfo.ImageCount = NUMBER_OF_SWAPCHAIN_IMAGES;
	initInfo.UseDynamicRendering = true;
	initInfo.MSAASamples = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1);
	initInfo.UseDynamicRendering = true;
	initInfo.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;
	ImGui_ImplVulkan_Init(&initInfo);

	ImGui_ImplVulkan_CreateFontsTexture();
	ImGui_ImplVulkan_DestroyFontsTexture();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;

	mSelectModelFileDialog = ImGui::FileBrowser::FileBrowser(ImGuiFileBrowserFlags_::ImGuiFileBrowserFlags_MultipleSelection, MODELS_PATH);
	mSelectModelFileDialog.SetTitle("Select GLTF / GLB File");
	mSelectModelFileDialog.SetTypeFilters({ ".glb", ".gltf" });

	mSelectSkyboxFileDialog = ImGui::FileBrowser::FileBrowser(ImGuiFileBrowserFlags_::ImGuiFileBrowserFlags_SelectDirectory, SKYBOXES_PATH);
	mSelectSkyboxFileDialog.SetTitle("Select Directory of Skybox Image");

	ImGui::StyleColorsDark();

	ImGui::SetNextWindowSize(ImVec2(0.2 * ImGui::GetMainViewport()->Size.x, ImGui::GetMainViewport()->Size.y));

	mGuiComponents.push_back(std::make_unique<CameraGuiComponent>(mRenderer, this, "Camera")); // Avoid slicing down to base class
	mGuiComponents.push_back(std::make_unique<SceneGuiComponent>(mRenderer, this, "Scene"));
	mGuiComponents.push_back(std::make_unique<MiscGuiComponent>(mRenderer, this, "Misc"));
}

void Gui::cleanup() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	mDescriptorPool.clear();
}

void Gui::imguiFrame() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetWindowSize().x, ImGui::GetMainViewport()->Size.y));

	if (ImGui::Begin("Renderer Options"), nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar) {
		if (ImGui::BeginTabBar("RendererOptionsTabBar", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_NoCloseWithMiddleMouseButton | ImGuiTabBarFlags_FittingPolicyResizeDown))
		{
			for (auto& component : mGuiComponents) {
				if (ImGui::BeginTabItem(component->mName.c_str(), nullptr, ImGuiTabItemFlags_NoCloseButton))
				{
					component->elements();
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}

	ImGui::Render();
}