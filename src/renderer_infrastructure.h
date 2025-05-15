#pragma once

#include <vk_pipelines.h>

class Renderer;

class RendererInfrastructure {
private:
    Renderer* mRenderer;

public:
    RendererInfrastructure(Renderer* renderer);

    void init();

    void initCommands();
    void initDescriptors();
    void initSyncStructures();

    void createSwapchain();
    void destroySwapchain();
    void resizeSwapchain();
    
    PipelineBundle* getMaterialPipeline(PipelineOptions pipelineOptions);
    void createMaterialPipeline(PipelineOptions pipelineOptions);
    void createComputePipeline(PipelineOptions pipelineOptions);
    void destroyPipelines();

    void cleanup();
};