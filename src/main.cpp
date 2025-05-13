#include <renderer.h>

int main()
{	
	Renderer renderer;
	renderer.init();
	renderer.run();
	renderer.cleanup();

	return 0;
}