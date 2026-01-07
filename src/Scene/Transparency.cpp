#include <Renderer/Renderer.h>
#include <Scene/Transparency.h>
#include <quill/LogMacros.h>

Transparency::Transparency(Renderer* renderer) : mRenderer(renderer) {}

void Transparency::initImages() {
    mAccumImage = mRenderer->mResources.createImage(
        mRenderer->mInfrastructure.mDrawImage.imageExtent, vk::Format::eR16G16B16A16Sfloat, vk::ImageUsageFlagBits::eColorAttachment, false, true
    );
    mRenderer->mCore.labelResourceDebug(mAccumImage.image, "AccumImage");
    mRenderer->mCore.labelResourceDebug(mAccumImage.imageView, "AccumImageView");
    LOG_INFO(mRenderer->mLogger, "Accumulation Image and Image View Created");

    mRevealageImage = mRenderer->mResources.createImage(
        mRenderer->mInfrastructure.mDrawImage.imageExtent, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eColorAttachment, false, true
    );
    mRenderer->mCore.labelResourceDebug(mRevealageImage.image, "RevealageImage");
    mRenderer->mCore.labelResourceDebug(mRevealageImage.imageView, "RevealageImageView");
    LOG_INFO(mRenderer->mLogger, "Revealage Image and Image View Created");

    mRenderer->mImmSubmit.mCallbacks.emplace_back([this](Renderer* renderer, vk::CommandBuffer cmd) {
        vkhelper::transitionImage(
            cmd,
            *mAccumImage.image,
            vk::ImageLayout::eUndefined,
            vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite
        );
        vkhelper::transitionImage(
            cmd,
            *mRevealageImage.image,
            vk::ImageLayout::eUndefined,
            vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite
        );
    });
}

void Transparency::init() { initImages(); }

void Transparency::resizeImages() {
    mAccumImage.cleanup();
    LOG_INFO(mRenderer->mLogger, "Accumulation Image and Image View Destroyed");
    mRevealageImage.cleanup();
    LOG_INFO(mRenderer->mLogger, "Revealage Image and Image View Destroyed");

    initImages();

    LOG_INFO(mRenderer->mLogger, "Transparency Images Resized");
}

void Transparency::cleanup() {
    mAccumImage.cleanup();
    LOG_INFO(mRenderer->mLogger, "Accumulation Image and Image View Destroyed");
    mRevealageImage.cleanup();
    LOG_INFO(mRenderer->mLogger, "Revealage Image and Image View Destroyed");
}