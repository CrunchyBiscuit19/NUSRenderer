#pragma once

class Renderer;

class GUI {
private:
	Renderer* mRenderer;

public:
	GUI(Renderer* renderer);

	void init();
	void cleanup();

	void imguiFrame();
};