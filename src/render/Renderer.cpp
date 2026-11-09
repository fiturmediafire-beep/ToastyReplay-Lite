#include "Renderer.hpp"

// Render feature stubbed out — eclipse.ffmpeg-api not yet compatible with Geode v6.
// Will be re-enabled once ffmpeg-api supports v6.

namespace toasty::renderer {
    void startRecording() {}
    void stopRecording() {}
    void captureFrame() {}
    bool isRecording() { return false; }
}
