#pragma once

#include <resource_manager.h>

#include <filesystem>
namespace fs = std::filesystem;

class Renderer;

class Skybox {
private:
	Renderer* mRenderer;

public:
	AllocatedImage mSkyboxImage;

	Skybox(Renderer* renderer);
	Skybox(Renderer* renderer, fs::path right, fs::path left, fs::path top, fs::path bottom, fs::path front, fs::path back);

	void loadSkyboxImage(fs::path right, fs::path left, fs::path top, fs::path bottom, fs::path front, fs::path back);

	void cleanup();
};