#pragma once

#include <Renderer/RendererResources.h>

class Renderer;

class Transparency {
	Renderer* mRenderer;

public:	
	AllocatedImage mAccumImage;
	AllocatedImage mRevealageImage;

	Transparency(Renderer* renderer);
	
	void init();
	void initImages();
    void resizeImages();

	void cleanup();
};
;