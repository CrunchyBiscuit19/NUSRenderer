#pragma once

#include <imgui.h>
#define NOMINMAX // imfilebrowser.h contains windows.h
#include <imfilebrowser.h>

class Renderer;

class GUI {
private:
	Renderer* mRenderer;

	ImGui::FileBrowser mSelectModelFileDialog;
	ImGui::FileBrowser mSelectSkyboxFileDialog;

public:
	GUI(Renderer* renderer);

	void init();
	void cleanup();

	void imguiFrame();
};