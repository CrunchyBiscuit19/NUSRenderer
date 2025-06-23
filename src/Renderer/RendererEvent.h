#pragma once

#include <SDL_events.h>

#include <vector>
#include <functional>

class Renderer;

class RendererEvent
{
	Renderer* mRenderer;
	std::vector<std::function<void(SDL_Event& e)>> mInputCallbacks;

public:
	RendererEvent(Renderer* renderer);

	void addEventCallback(const std::function<void(SDL_Event& e)>& inputCallback);
	void executeEventCallbacks(SDL_Event& e) const;
};
