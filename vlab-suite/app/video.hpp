// video.hpp -- plays an .mp4 (or any file Windows can decode) as a stream of BGRA frames, for the dashboard cover.
// Uses the Media Foundation source reader that is already part of Windows: no codecs or libraries to install.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vlab {

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();
    bool open(const std::string& path);      // false if the file is missing or cannot be decoded
    void close();
    bool isOpen() const;
    // Advances the playback clock by dt seconds and decodes at most one frame that is due. Returns true if pixels() changed. Loops forever.
    bool advance(double dt);
    int width() const { return w_; }
    int height() const { return h_; }
    const std::vector<uint32_t>& pixels() const { return pix_; }   // top-down, BGRA, alpha forced to 255

private:
    struct Impl;
    Impl* p_ = nullptr;
    int w_ = 0, h_ = 0;
    std::vector<uint32_t> pix_;
};

}  // namespace vlab
