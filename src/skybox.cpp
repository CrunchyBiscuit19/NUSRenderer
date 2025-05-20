#include <renderer.h>
#include <skybox.h>

#include <stb_image.h>

Skybox::Skybox(Renderer* renderer) :
    mRenderer(renderer)
{}

Skybox::Skybox(Renderer* renderer, fs::path right, fs::path left, fs::path top, fs::path bottom, fs::path front, fs::path back) :
	mRenderer(renderer)
{
	loadSkyboxImage(right, left, top, bottom, front, back);
}

void Skybox::loadSkyboxImage(fs::path right, fs::path left, fs::path top, fs::path bottom, fs::path front, fs::path back)
{
    std::vector<fs::path> skyboxImagePaths = {right, left, top, bottom, front, back};
    std::vector<std::byte> skyboxImageData(MAX_IMAGE_SIZE);

    int width, height, nrChannels;
    int offset = 0;

    for (auto& path : skyboxImagePaths) {
        if (unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &nrChannels, 4)) {
            int imageSize = width * height * 4;
            std::memcpy(skyboxImageData.data() + offset, data, imageSize);
            offset += imageSize;
            stbi_image_free(data);
        }
    }

    mSkyboxImage = mRenderer->mResourceManager.createImage(skyboxImageData.data(), vk::Extent3D {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true, true);
}

void Skybox::cleanup()
{
    mSkyboxImage.cleanup();
}