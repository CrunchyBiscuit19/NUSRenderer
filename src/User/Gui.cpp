#include <User/Gui.h>
#include <Renderer/Renderer.h>

#include <fmt/core.h>
#include <magic_enum.hpp>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <quill/LogMacros.h>

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
	ImGui::Text("Position: [%.1f, %.1f, %.1f]", mRenderer->mCamera.mPosition.x, mRenderer->mCamera.mPosition.y,
		mRenderer->mCamera.mPosition.z);
	ImGui::Text("Pitch / Yaw: [%.1f, %.1f]", mRenderer->mCamera.mPitch, mRenderer->mCamera.mYaw);
	ImGui::Text("Speed: %.1f / %.1f", mRenderer->mCamera.mSpeed, MAX_CAMERA_SPEED);
}

void Gui::SceneGuiComponent::elements()
{
	for (auto& model : mRenderer->mScene.mModelsCache | std::views::values)
	{
		const auto name = model.mName;
		ImGui::PushStyleColor(ImGuiCol_Header, static_cast<ImVec4>(IMGUI_HEADER_GREEN));
		if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button(fmt::format("Add Instance##{}", name).c_str()))
			{
				model.createInstanceAtCamera(mRenderer->mCamera);
			}
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(IMGUI_BUTTON_RED));
			if (ImGui::Button(fmt::format("Delete Model##{}", name).c_str()))
			{
				model.markDelete();
			}
			ImGui::PopStyleColor();

			for (auto& instance : model.mInstances)
			{
				if (ImGui::TreeNode(fmt::format("{}-{}", model.mName, instance.mId).c_str()))
				{
					ImGui::PushID(fmt::format("{}-{}", model.mName, instance.mId).c_str());

					glm::vec3 translation, rotation, scale;
					ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(instance.mData.transformMatrix), glm::value_ptr(translation), glm::value_ptr(rotation), glm::value_ptr(scale));
					for (int i = 0; i < 3; i++) {
						rotation[i] = glm::radians(rotation[i]);
					}
					ImGui::InputFloat3("Translation", glm::value_ptr(translation), "%.3f", ImGuiInputTextFlags_ReadOnly);
					ImGui::InputFloat3("Rotation", glm::value_ptr(rotation), "%.3f", ImGuiInputTextFlags_ReadOnly);
					ImGui::InputFloat3("Scale", glm::value_ptr(scale), "%.3f", ImGuiInputTextFlags_ReadOnly);

					ImGui::PopID();
					ImGui::TreePop();
				}
			}
		}
		ImGui::PopStyleColor();
	}

	if (ImGui::CollapsingHeader("Sunlight", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::ColorEdit3("Ambient Color", glm::value_ptr(mRenderer->mScene.mPerspective.mData.ambientColor));
		ImGui::ColorEdit3("Sunlight Color", glm::value_ptr(mRenderer->mScene.mPerspective.mData.sunlightColor));
		ImGui::SliderFloat3("Sunlight Direction",
			glm::value_ptr(mRenderer->mScene.mPerspective.mData.sunlightDirection), 0.f, 1.f);
		ImGui::InputFloat("Sunlight Power", &mRenderer->mScene.mPerspective.mData.sunlightDirection[3]);
	}
	if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Button("Change Skybox"))
		{
			mGui->mSelectSkyboxFileBrowser.Open();
		}
		ImGui::SameLine();
		if (ImGui::Button("Toggle Skybox"))
		{
			mRenderer->mScene.mSkybox.mActive = !mRenderer->mScene.mSkybox.mActive;
		}
	}

	mGui->mSelectSkyboxFileBrowser.Display();
	if (mGui->mSelectSkyboxFileBrowser.HasSelected())
	{
		std::filesystem::path selectedSkyboxDir = mGui->mSelectSkyboxFileBrowser.GetSelected();
		mRenderer->mScene.mSkybox.updateImage(selectedSkyboxDir);
		mGui->mSelectSkyboxFileBrowser.ClearSelected();
	}
}

void Gui::MiscGuiComponent::elements()
{
	if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("VALIDATION LAYERS: %s", (USE_VALIDATION_LAYERS ? "ON" : "OFF"));
		ImGui::Text("Frame Time:  %fms", mRenderer->mStats.mFrameTime);
		ImGui::Text("Draw Time:  %fms", mRenderer->mStats.mDrawTime);
		ImGui::Text("Update Time: %fms", mRenderer->mStats.mSceneUpdateTime);
		ImGui::Text("Draws: %i", mRenderer->mStats.mDrawCallCount);
	}
	if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("[G] Toggle GUI");
		ImGui::Text("[Alt + Enter] Toggle Borderless Fullscreen");
		ImGui::Text("[C] Change Camera Mode");
		ImGui::Text("[Mouse Scroll] Control Camera Speed");
		ImGui::Text("[Left Click] Select / Deselect Object");
		ImGui::Text("[Right Click] Enter / Leave Window");
		ImGui::Text("[Ctrl + I] Import Model");
		ImGui::Text("[T] Switch Transform Mode");
		ImGui::Text("[Del] Delete Clicked Instance");
	}
}

void Gui::createDockSpace()
{
	static ImGuiDockNodeFlags dockSpaceFlags =
		ImGuiDockNodeFlags_NoDockingOverCentralNode |
		ImGuiDockNodeFlags_PassthruCentralNode;
	static ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;

	static ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	static ImGuiID mainDockSpace = ImGui::DockSpaceOverViewport(0, mainViewport, dockSpaceFlags);

	ImGui::SetNextWindowPos(mainViewport->WorkPos);
	ImGui::SetNextWindowSize(mainViewport->WorkSize);
	ImGui::SetNextWindowViewport(mainViewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("DockSpace Window", &mCollapsed, windowFlags);
	ImGui::DockSpace(mainDockSpace, ImVec2(0.0f, 0.0f), dockSpaceFlags);
	ImGui::End();
	ImGui::PopStyleVar(3);
}

void Gui::createRendererOptionsWindow() const
{
	if (mCollapsed) return;
	if (ImGui::Begin("Renderer Options", nullptr, ImGuiWindowFlags_NoDecoration)) {
		if (!ImGui::IsWindowCollapsed()) {
			if (ImGui::BeginTabBar("RendererOptionsTabBar",
				ImGuiTabBarFlags_Reorderable |
				ImGuiTabBarFlags_NoCloseWithMiddleMouseButton |
				ImGuiTabBarFlags_FittingPolicyResizeDown))
			{
				for (auto& component : mGuiComponents)
				{
					if (ImGui::BeginTabItem(component->mName.c_str(), nullptr, ImGuiTabItemFlags_NoCloseButton))
					{
						component->elements();
						ImGui::EndTabItem();
					}
				}
				ImGui::EndTabBar();
			}
		}
	}
	ImGui::End();
}

Gui::Gui(Renderer* renderer) :
	mRenderer(renderer),
	mDescriptorPool(nullptr),
	mDescriptorSet(nullptr),
	mCollapsed(false)
{
}

void Gui::init()
{
	initDescriptors();
	initBackend();
	initFileBrowsers();
	initComponents();
	initKeyBinding();
}

void Gui::initDescriptors()
{
	std::vector<vk::DescriptorPoolSize> poolSizes = {
		{vk::DescriptorType::eSampler, 100},
		{vk::DescriptorType::eCombinedImageSampler, 100},
		{vk::DescriptorType::eSampledImage, 100},
		{vk::DescriptorType::eStorageImage, 100},
		{vk::DescriptorType::eUniformTexelBuffer, 100},
		{vk::DescriptorType::eStorageTexelBuffer, 100},
		{vk::DescriptorType::eUniformBuffer, 100},
		{vk::DescriptorType::eUniformBufferDynamic, 100},
		{vk::DescriptorType::eStorageBuffer, 100},
		{vk::DescriptorType::eStorageBufferDynamic, 100},
		{vk::DescriptorType::eInputAttachment, 100},
	};
	vk::DescriptorPoolCreateInfo poolInfo = {};
	poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	poolInfo.maxSets = 100;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	mDescriptorPool = mRenderer->mCore.mDevice.createDescriptorPool(poolInfo);

	LOG_INFO(mRenderer->mLogger, "ImGui Descriptor Pool Created");
}

void Gui::initBackend() const
{
	ImGui::CreateContext();
	ImGui_ImplSDL2_InitForVulkan(mRenderer->mCore.mWindow);

	LOG_INFO(mRenderer->mLogger, "ImGui SDL2 Vulkan Initialized");

	vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &mRenderer->mInfrastructure.mSwapchainBundle.mFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = mRenderer->mInfrastructure.mDepthImage.imageFormat;

	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = *mRenderer->mCore.mInstance;
	initInfo.PhysicalDevice = *mRenderer->mCore.mChosenGPU;
	initInfo.Device = *mRenderer->mCore.mDevice;
	initInfo.Queue = *mRenderer->mCore.mGraphicsQueue;
	initInfo.DescriptorPool = *mDescriptorPool;
	initInfo.MinImageCount = NUMBER_OF_SWAPCHAIN_IMAGES;
	initInfo.ImageCount = NUMBER_OF_SWAPCHAIN_IMAGES;
	initInfo.UseDynamicRendering = true;
	initInfo.MSAASamples = static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1);
	initInfo.UseDynamicRendering = true;
	initInfo.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;
	ImGui_ImplVulkan_Init(&initInfo);

	LOG_INFO(mRenderer->mLogger, "ImGui Implementation Vulkan Initialized");

	ImGui_ImplVulkan_CreateFontsTexture();
	ImGui_ImplVulkan_DestroyFontsTexture();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;

	LOG_INFO(mRenderer->mLogger, "ImGui Configured");
}

void Gui::initFileBrowsers()
{
	mSelectModelFileBrowser = ImGui::FileBrowser::FileBrowser(ImGuiFileBrowserFlags_MultipleSelection, MODELS_PATH);
	mSelectModelFileBrowser.SetTitle("Select GLTF / GLB File");
	mSelectModelFileBrowser.SetTypeFilters({ ".glb", ".gltf" });

	mSelectSkyboxFileBrowser = ImGui::FileBrowser::FileBrowser(ImGuiFileBrowserFlags_SelectDirectory, SKYBOXES_PATH);
	mSelectSkyboxFileBrowser.SetTitle("Select Directory of Skybox Image");

	LOG_INFO(mRenderer->mLogger, "ImGui FileBrowsers Created");
}

void Gui::initComponents()
{

	mGuiComponents.push_back(std::make_unique<CameraGuiComponent>(mRenderer, this, "Camera"));
	mGuiComponents.push_back(std::make_unique<SceneGuiComponent>(mRenderer, this, "Scene"));
	mGuiComponents.push_back(std::make_unique<MiscGuiComponent>(mRenderer, this, "Misc"));

	LOG_INFO(mRenderer->mLogger, "ImGui Gui Components Added");
}

void Gui::initKeyBinding()
{
	mRenderer->mEventHandler.addEventCallback([this](SDL_Event& e) -> void
	{
		const Uint8* keyState = SDL_GetKeyboardState(nullptr);

		if (keyState[SDL_SCANCODE_G] && e.type == SDL_KEYDOWN && !e.key.repeat) {
			mCollapsed = !mCollapsed;
		}

		if (keyState[SDL_SCANCODE_T] && e.type == SDL_KEYDOWN && !e.key.repeat) {
			mRenderer->mScene.mPicker.changeImguizmoOperation();
		}
	});
}

void Gui::updateFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	createDockSpace();
	createRendererOptionsWindow();
	mRenderer->mScene.mPicker.imguizmoFrame();

	mSelectModelFileBrowser.Display();
	if (mSelectModelFileBrowser.HasSelected())
	{
		auto selectedFiles = mSelectModelFileBrowser.GetMultiSelected();
		mRenderer->mScene.loadModels(selectedFiles);
		mSelectModelFileBrowser.ClearSelected();
	}

	ImGui::Render();
}

void Gui::cleanup()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	mDescriptorPool.clear();

	LOG_INFO(mRenderer->mLogger, "ImGui Destroyed");
}