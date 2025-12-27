#include <Renderer/Renderer.h>

#include <Utils/Types.h>

u32 main() {
    Renderer renderer;
    renderer.init();
    renderer.run();
    renderer.cleanup();

    return 0;
}
