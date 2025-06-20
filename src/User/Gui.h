#pragma once

#include <imgui.h>
#define NOMINMAX // imfilebrowser.h contains windows.h
#include <imfilebrowser.h>

class Renderer;
class Gui;

struct GuiComponent
{
protected:
	Renderer* mRenderer;
	Gui* mGui;

public:
	std::string mName;

	GuiComponent(Renderer* renderer, Gui* gui, std::string name);
	virtual void elements() = 0;
};

class Gui
{
	struct CameraGuiComponent : GuiComponent
	{
		CameraGuiComponent(Renderer* renderer, Gui* gui, std::string name) : GuiComponent(renderer, gui, name)
		{
		}

		void elements() override;
	};

	struct SceneGuiComponent : GuiComponent
	{
		SceneGuiComponent(Renderer* renderer, Gui* gui, std::string name) : GuiComponent(renderer, gui, name)
		{
		}

		void elements() override;
	};

	struct MiscGuiComponent : GuiComponent
	{
		MiscGuiComponent(Renderer* renderer, Gui* gui, std::string name) : GuiComponent(renderer, gui, name)
		{
		}

		void elements() override;
	};

	Renderer* mRenderer;

	vk::raii::DescriptorPool mDescriptorPool;
	vk::raii::DescriptorSet mDescriptorSet;

	ImGui::FileBrowser mSelectModelFileBrowser;
	ImGui::FileBrowser mSelectSkyboxFileBrowser;

	bool mCollapsed;
	std::vector<std::unique_ptr<GuiComponent>> mGuiComponents;

	static void createDockSpace();
	void createRendererOptionsMenu() const;

public:
	Gui(Renderer* renderer);

	void init();
	void initDescriptors();
	void initBackend();
	void initFileBrowsers();
	void initComponents();
	void initKeyBinding() const;

	void imguiFrame() const;

	void cleanup();

};
