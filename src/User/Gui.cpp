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
	ImGui::SliderFloat("Speed", &mRenderer->mCamera.mSpeed, 0.f, 100.f, "%.2f");
}

void Gui::SceneGuiComponent::elements()
{
	if (ImGui::Button("Add Model"))
	{
		mGui->mSelectModelFileBrowser.Open();
	}
	for (auto& model : mRenderer->mRendererScene.mModels | std::views::values)
	{
		const auto name = model.mName;
		ImGui::PushStyleColor(ImGuiCol_Header, static_cast<ImVec4>(IMGUI_HEADER_GREEN));
		if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button(fmt::format("Add Instance##{}", name).c_str()))
			{
				model.createInstance(TransformData{
					mRenderer->mCamera.mPosition + mRenderer->mCamera.getDirectionVector(), glm::vec3(), 1.f
					});
				model.mReloadLocalInstancesBuffer = true;
				mRenderer->mRendererScene.mFlags.instanceAddedFlag = true;
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

					if (ImGui::InputFloat3("Translation", glm::value_ptr(instance.mTransformComponents.translation)))
					{
						model.mReloadLocalInstancesBuffer = true;
						mRenderer->mRendererScene.mFlags.reloadMainInstancesBuffer = true;
					}
					if (ImGui::SliderFloat3("Pitch / Yaw / Roll",
						glm::value_ptr(instance.mTransformComponents.rotation), -glm::pi<float>(),
						glm::pi<float>()))
					{
						model.mReloadLocalInstancesBuffer = true;
						mRenderer->mRendererScene.mFlags.reloadMainInstancesBuffer = true;
					}
					if (ImGui::SliderFloat("Scale", &instance.mTransformComponents.scale, 0.f, 100.f))
					{
						model.mReloadLocalInstancesBuffer = true;
						mRenderer->mRendererScene.mFlags.reloadMainInstancesBuffer = true;
					}

					ImGui::PushStyleColor(ImGuiCol_Button, static_cast<ImVec4>(IMGUI_BUTTON_RED));
					if (ImGui::Button("Delete Instance"))
					{
						instance.mDeleteSignal = true;
						model.mReloadLocalInstancesBuffer = true;
						mRenderer->mRendererScene.mFlags.instanceDestroyedFlag = true;
					}
					ImGui::PopStyleColor();

					ImGui::PopID();
					ImGui::TreePop();
				}
			}
		}
		ImGui::PopStyleColor();
	}

	if (ImGui::CollapsingHeader("Sunlight", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::ColorEdit3("Ambient Color", glm::value_ptr(mRenderer->mRendererScene.mPerspective.mData.ambientColor));
		ImGui::ColorEdit3("Sunlight Color", glm::value_ptr(mRenderer->mRendererScene.mPerspective.mData.sunlightColor));
		ImGui::SliderFloat3("Sunlight Direction",
			glm::value_ptr(mRenderer->mRendererScene.mPerspective.mData.sunlightDirection), 0.f, 1.f);
		ImGui::InputFloat("Sunlight Power", &mRenderer->mRendererScene.mPerspective.mData.sunlightDirection[3]);
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
			mRenderer->mRendererScene.mSkybox.mActive = !mRenderer->mRendererScene.mSkybox.mActive;
		}
	}

	mGui->mSelectSkyboxFileBrowser.Display();
	if (mGui->mSelectSkyboxFileBrowser.HasSelected())
	{
		std::filesystem::path selectedSkyboxDir = mGui->mSelectSkyboxFileBrowser.GetSelected();
		mRenderer->mRendererScene.mSkybox.updateImage(selectedSkyboxDir);
		mGui->mSelectSkyboxFileBrowser.ClearSelected();
	}

	mGui->mSelectModelFileBrowser.Display();
	if (mGui->mSelectModelFileBrowser.HasSelected())
	{
		auto selectedFiles = mGui->mSelectModelFileBrowser.GetMultiSelected();
		mRenderer->mRendererScene.loadModels(selectedFiles);
		mGui->mSelectModelFileBrowser.ClearSelected();
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
		ImGui::Text("[F11] Change Camera Mode");
		ImGui::Text("[F10] Toggle Borderless Fullscreen");
		ImGui::Text("[F9] Toggle GUI");
		ImGui::Text("[Right Click] Change Mouse Mode");
	}
	if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Mouse Last Clicked: (%d, %d)", mRenderer->mRendererScene.mPicker.mMouseClickLocation.first, mRenderer->mRendererScene.mPicker.mMouseClickLocation.second);
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

void Gui::createRendererOptionsWindow()
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
	mDescriptorPool = mRenderer->mRendererCore.mDevice.createDescriptorPool(poolInfo);

	LOG_INFO(mRenderer->mLogger, "ImGui Descriptor Pool Created");
}

void Gui::initBackend() const
{
	ImGui::CreateContext();
	ImGui_ImplSDL2_InitForVulkan(mRenderer->mRendererCore.mWindow);

	LOG_INFO(mRenderer->mLogger, "ImGui SDL2 Vulkan Initialized");

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
	mRenderer->mRendererEvent.addEventCallback([this](SDL_Event& e) -> void
	{
		const Uint8* keyState = SDL_GetKeyboardState(nullptr);

		if (keyState[SDL_SCANCODE_F9] && e.type == SDL_KEYDOWN && !e.key.repeat) {
			mCollapsed = !mCollapsed;
		}
	});
}

void Gui::imguiFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	createDockSpace();
	createRendererOptionsWindow();

	ImGui::Render();
}

void Gui::cleanup()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	mDescriptorPool.clear();

	LOG_INFO(mRenderer->mLogger, "ImGui Destroyed");
}