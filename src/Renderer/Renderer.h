#pragma once

#include <Renderer/RendererCore.h>
#include <Renderer/RendererInfrastructure.h>
#include <Renderer/RendererResources.h>
#include <Renderer/RendererScene.h>
#include <Renderer/RendererEvent.h>
#include <Utils/ImmSubmit.h>
#include <User/Gui.h>
#include <Data/Camera.h>
#include <Utils/Helper.h>

#include <quill/Logger.h>

struct RendererStats
{
	float mFrameTime;
	float mDrawTime;
	int mDrawCallCount;
	float mSceneUpdateTime;

	void reset()
	{
		mDrawCallCount = 0;
	}
};

enum class PassType
{
	Cull,
	ClearScreen,
	Pick,
	PickClear,
	PickDraw,
	PickPick,
	Geometry,
	Skybox,
	ResolveMSAA,
	IntermediateToSwapchain,
	ImGui
};

struct Pass
{
	static Renderer* renderer;
	std::function<void(vk::CommandBuffer)> function;

	Pass(const std::function<void(vk::CommandBuffer)>& function) :
		function(function)
	{
	}

	void execute(vk::CommandBuffer cmd) const
	{
		function(cmd);
	}
};

enum class TransitionType
{
	PickerGeneralIntoColorAttachment,
	PickerColorAttachmentIntoGeneral,
	IntermediateTransferSrcIntoColorAttachment,
	IntermediateColorAttachmentIntoTransferSrc,
	SwapchainPresentIntoTransferDst,
	SwapchainTransferDstIntoColorAttachment,
	SwapchainColorAttachmentIntoPresent,
};

struct Transition
{
	vk::PipelineStageFlags2 srcStageMask;
	vk::AccessFlags2 srcAccessMask;
	vk::PipelineStageFlags2 dstStageMask;
	vk::AccessFlags2 dstAccessMask;
	vk::ImageLayout currentLayout;
	vk::ImageLayout newLayout;

	Transition(vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask,
	           vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask, vk::ImageLayout currentLayout,
	           vk::ImageLayout newLayout) :
		srcStageMask(srcStageMask),
		srcAccessMask(srcAccessMask),
		dstStageMask(dstStageMask),
		dstAccessMask(dstAccessMask),
		currentLayout(currentLayout),
		newLayout(newLayout)
	{
	}

	void execute(vk::CommandBuffer cmd, vk::Image image)
	{
		vkhelper::transitionImage(
			cmd,
			image,
			srcStageMask,
			srcAccessMask,
			dstStageMask,
			dstAccessMask,
			currentLayout,
			newLayout
		);
	}
};

class Renderer
{
public:
	bool mIsInitialized{false};
	bool mStopRendering{false};

	RendererStats mStats;

	RendererCore mCore;
	RendererInfrastructure mInfrastructure;
	RendererResources mResources;
	RendererScene mScene;
	RendererEvent mEventHandler;
	ImmSubmit mImmSubmit;
	Gui mGui;
	Camera mCamera;
	quill::Logger* mLogger;

	std::unordered_map<PassType, Pass> mPasses;
	std::unordered_map<TransitionType, Transition> mTransitions;

	Renderer();

	void init();
	void initLogger();
	void initComponents();
	void initPasses();
	void initTransitions();

	void run();
	void perFrameUpdate();
	void draw();

	void cleanup();
};
