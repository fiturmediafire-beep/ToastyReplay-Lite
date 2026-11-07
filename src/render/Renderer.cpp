#include "Renderer.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <eclipse.ffmpeg-api/include/recorder.hpp>

#include <filesystem>
#include <string>
#include <vector>
#include <ctime>

using namespace geode::prelude;

namespace {
    bool s_recording = false;
    ffmpeg::Recorder s_recorder;
    int s_width = 0;
    int s_height = 0;
    int s_fps = 60;

    std::string getOutputPath() {
        // Konversi geode::Path ke std::filesystem::path terlebih dahulu
        std::filesystem::path base = std::filesystem::path(geode::dirs::getSaveDir().string()) / "toastyreplay" / "renders";
        std::filesystem::create_directories(base);

        // Nama file pakai timestamp
        std::time_t t = std::time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof(buf), "render_%Y%m%d_%H%M%S.mp4", std::localtime(&t));
        return (base / buf).string();
    }
}

namespace toasty::renderer {
    void startRecording() {
        if (s_recording) return;

        auto director = CCDirector::sharedDirector();
        if (!director) return;

        auto winSize = director->getWinSizeInPixels();
        s_width  = static_cast<int>(winSize.width);
        s_height = static_cast<int>(winSize.height);

        ffmpeg::RenderSettings settings;
        settings.m_width       = s_width;
        settings.m_height      = s_height;
        settings.m_fps         = s_fps;
        settings.m_bitrate     = 8000000; // 8 Mbps
        settings.m_codec       = "libx264";
        settings.m_pixelFormat = ffmpeg::PixelFormat::RGB0;
        settings.m_outputFile  = getOutputPath();

        auto initResult = s_recorder.init(settings);
        if (initResult.isErr()) {
            log::error("[ToastyRender] Failed to init recorder: {}", initResult.unwrapErr());
            return;
        }
        s_recording = true;

        log::info("[ToastyRender] Recording started → {}", settings.m_outputFile);
    }

    void stopRecording() {
        if (!s_recording) return;
        s_recording = false;

        s_recorder.stop();
        log::info("[ToastyRender] Recording stopped, file saved.");

        geode::Notification::create("Render saved!", NotificationIcon::Success)->show();
    }

    void captureFrame() {
        if (!s_recording) return;

        auto director = CCDirector::sharedDirector();
        if (!director) return;

        // Buat RenderTexture seukuran layar
        auto rt = CCRenderTexture::create(s_width, s_height, kCCTexture2DPixelFormat_RGBA8888);
        if (!rt) return;

        rt->begin();
        director->getRunningScene()->visit();
        rt->end();

        // Ambil pixel data dari texture
        auto img = rt->newCCImage(false);
        if (!img) return;

        auto data = img->getData();
        auto len  = img->getDataLen();
        if (!data || len == 0) {
            img->release();
            return;
        }

        std::vector<uint8_t> frame(data, data + len);
        auto writeResult = s_recorder.writeFrame(frame);
        if (writeResult.isErr()) {
            log::warn("[ToastyRender] writeFrame failed: {}", writeResult.unwrapErr());
        }

        img->release();
    }

    bool isRecording() {
        return s_recording;
    }
}

// Hook PlayLayer untuk capture tiap frame saat replay play
class $modify(RendererPlayLayer, PlayLayer) {
    void postUpdate(float dt) override {
        PlayLayer::postUpdate(dt);
        toasty::renderer::captureFrame();
    }
};
