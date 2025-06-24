#include <Renderer/RendererEvent.h>

RendererEvent::RendererEvent(Renderer* renderer) :
	mRenderer(renderer)
{
}

void RendererEvent::addEventCallback(const std::function<void(SDL_Event& e)>& inputCallback)
{
	mEventCallbacks.push_back(inputCallback);
}

void RendererEvent::executeEventCallbacks(SDL_Event& e) const
{
	for (const auto& inputCallback : mEventCallbacks)
	{
		inputCallback(e);
	}
}
