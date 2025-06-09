#pragma once

#include <Renderer/RendererCore.h>
#include <Renderer/RendererInfrastructure.h>
#include <Renderer/RendererResources.h>
#include <Renderer/RendererScene.h>
#include <Renderer/RendererEvent.h>
#include <Utils/ImmSubmit.h>
#include <User/Gui.h>
#include <Data/Camera.h>

struct RendererStats {
	float mFrametime;
	float mDrawTime;
	int mDrawCallCount;
	float mSceneUpdateTime;
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

	Renderer();

	void init();
	void run();
	void cleanup();

	void setViewportScissors(vk::CommandBuffer cmd);

	void endOfFrameUpdate();

	void draw();
	
	void cullRenderItems(vk::CommandBuffer cmd);

	void drawClearScreen(vk::CommandBuffer cmd);
	void drawGeometry(vk::CommandBuffer cmd);
	void drawSkybox(vk::CommandBuffer cmd);
	
	void resolveMsaa(vk::CommandBuffer cmd);
	
	void drawGui(vk::CommandBuffer cmd, vk::ImageView swapchainImageView);

};