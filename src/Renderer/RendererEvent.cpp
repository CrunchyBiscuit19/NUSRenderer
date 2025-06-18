#include <Renderer/RendererEvent.h>

RendererEvent::RendererEvent(Renderer* renderer) :
	mRenderer(renderer)
{
}

void RendererEvent::addEventCallback(std::function<void(SDL_Event& e)> inputCallback)
{
	mInputCallbacks.push_back(inputCallback);
}

void RendererEvent::executeEventCallbacks(SDL_Event& e)
{
	for (auto inputCallback : mInputCallbacks)
	{
		inputCallback(e);
	}
}
