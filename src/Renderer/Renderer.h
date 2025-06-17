#pragma once

#include <Renderer/RendererCore.h>
#include <Renderer/RendererInfrastructure.h>
#include <Renderer/RendererResources.h>
#include <Renderer/RendererScene.h>
#include <Renderer/RendererEvent.h>
#include <Utils/ImmSubmit.h>
#include <User/Gui.h>
#include <Data/Camera.h>

#include <quill/Logger.h>
#include <quill/LogMacros.h>

struct RendererStats {
	float mFrameTime;
	float mDrawTime;
	int mDrawCallCount;
	float mSceneUpdateTime;

	void reset() {
		mDrawCallCount = 0;
	}
};

enum class PassType {
	Cull,
	ClearScreen,
	Pick,
	Geometry,
	Skybox,
	ResolveMSAA,
	ImGui
};

struct Pass {
	static Renderer* mRenderer;
	std::function<void(vk::CommandBuffer)> mFunction;

	Pass(std::function<void(vk::CommandBuffer)> function) :
		mFunction(function)
	{}

	void execute(vk::CommandBuffer cmd) {
		mFunction(cmd);
	}
};

class Renderer {
public:
	bool mIsInitialized{ false };
	bool mStopRendering{ false };

	RendererStats mStats;

	RendererCore mRendererCore;
	RendererInfrastructure mRendererInfrastructure;
	RendererResources mRendererResources;
	RendererScene mRendererScene;
	ImmSubmit mImmSubmit;
	Gui mGUI;
	Camera mCamera;
	RendererEvent mRendererEvent;
	quill::Logger* mLogger;

	std::unordered_map<PassType, Pass> mPasses;

	Renderer();

	void init();
	void initLogger();
	void initComponents();
	void initPasses();

	void run();
	void perFrameUpdate();
	void draw();
	
	void cleanup();





	void cullRenderItems(vk::CommandBuffer cmd);

	void clearScreen(vk::CommandBuffer cmd);

	//void drawPick(vk::CommandBuffer cmd);
	void drawGeometry(vk::CommandBuffer cmd);
	void drawSkybox(vk::CommandBuffer cmd);
	
	void resolveMsaa(vk::CommandBuffer cmd);
	
	void drawGui(vk::CommandBuffer cmd, vk::ImageView swapchainImageView);

};