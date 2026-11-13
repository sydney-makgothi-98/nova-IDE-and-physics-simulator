// encoder.hpp -- "direct to video": writes the picture on screen into an H.264 .mp4 file,
// using the video encoder that is already built into Windows (Media Foundation Sink Writer).
// No ffmpeg or any other install needed.
//
//   VideoEncoder enc;
//   enc.open("movie.mp4", 1000, 700, 60, error);     // width/height must be even
//   enc.addFrame(pixels, rowBytes, 1000, 700);        // call once per video frame, top-down BGRA pixels
//   enc.close();                                      // finishes the file (mp4 is unplayable without this!)
#pragma once
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

class VideoEncoder {
public:
    ~VideoEncoder() { if (writer_) close(); }
    bool isOpen() const { return writer_ != nullptr; }
    int  framesWritten() const { return (int)frame_; }
    double seconds() const { return fps_ > 0 ? (double)frame_ / fps_ : 0.0; }

    bool open(const std::string& path, int width, int height, int fps, std::string& err, int bitrate = 8000000) {
        if (writer_) { err = "already recording"; return false; }
        if (width < 16 || height < 16 || (width & 1) || (height & 1)) { err = "video size must be even and at least 16"; return false; }
        if (!startup(err)) return false;

        WCHAR wpath[1024];
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath, 1024);

        IMFAttributes* attr = nullptr;
        MFCreateAttributes(&attr, 2);
        attr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);     // use the GPU encoder if there is one
        attr->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);           // never skip frames to "keep up"
        HRESULT hr = MFCreateSinkWriterFromURL(wpath, nullptr, attr, &writer_);
        attr->Release();
        if (FAILED(hr)) { err = hrText("could not create the .mp4 file", hr); writer_ = nullptr; return false; }

        // 1) what we want to STORE: H.264 video
        IMFMediaType* out = nullptr;
        MFCreateMediaType(&out);
        out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        out->SetUINT32(MF_MT_AVG_BITRATE, (UINT32)bitrate);
        out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        out->SetUINT64(MF_MT_FRAME_SIZE, pack((UINT32)width, (UINT32)height));
        out->SetUINT64(MF_MT_FRAME_RATE, pack((UINT32)fps, 1));
        out->SetUINT64(MF_MT_PIXEL_ASPECT_RATIO, pack(1, 1));
        hr = writer_->AddStream(out, &stream_);
        out->Release();
        if (FAILED(hr)) return fail("no H.264 encoder available", hr, err);

        // 2) what we will FEED it: plain 32-bit RGB pictures
        IMFMediaType* in = nullptr;
        MFCreateMediaType(&in);
        in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        in->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        in->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        in->SetUINT64(MF_MT_FRAME_SIZE, pack((UINT32)width, (UINT32)height));
        in->SetUINT64(MF_MT_FRAME_RATE, pack((UINT32)fps, 1));
        in->SetUINT64(MF_MT_PIXEL_ASPECT_RATIO, pack(1, 1));
        hr = writer_->SetInputMediaType(stream_, in, nullptr);
        in->Release();
        if (FAILED(hr)) return fail("encoder rejected the picture format", hr, err);

        hr = writer_->BeginWriting();
        if (FAILED(hr)) return fail("could not start writing", hr, err);

        w_ = width; h_ = height; fps_ = fps; frame_ = 0;
        return true;
    }

    // pixels: top-down 32-bit (B,G,R,x) rows, `rowBytes` apart. Only the left w_ x h_ block is stored.
    bool addFrame(const void* pixels, int rowBytes) {
        if (!writer_) return false;
        const DWORD bytes = (DWORD)(w_ * h_ * 4);
        IMFMediaBuffer* buf = nullptr;
        if (FAILED(MFCreateMemoryBuffer(bytes, &buf))) return false;
        BYTE* dst = nullptr;
        buf->Lock(&dst, nullptr, nullptr);
        const BYTE* src = (const BYTE*)pixels;
        for (int y = 0; y < h_; ++y)                                   // Media Foundation wants RGB bottom-up: flip rows
            std::memcpy(dst + (size_t)(h_ - 1 - y) * w_ * 4, src + (size_t)y * rowBytes, (size_t)w_ * 4);
        buf->Unlock();
        buf->SetCurrentLength(bytes);

        IMFSample* sample = nullptr;
        MFCreateSample(&sample);
        sample->AddBuffer(buf);
        sample->SetSampleTime(frame_ * 10000000LL / fps_);             // times are in units of 100 nanoseconds
        sample->SetSampleDuration(10000000LL / fps_);
        HRESULT hr = writer_->WriteSample(stream_, sample);
        sample->Release();
        buf->Release();
        if (FAILED(hr)) return false;
        ++frame_;
        return true;
    }

    bool close() {
        if (!writer_) return false;
        HRESULT hr = writer_->Finalize();                              // writes the index so players can open the file
        writer_->Release();
        writer_ = nullptr;
        return SUCCEEDED(hr);
    }

private:
    IMFSinkWriter* writer_ = nullptr;
    DWORD stream_ = 0;
    int w_ = 0, h_ = 0, fps_ = 0;
    long long frame_ = 0;

    static UINT64 pack(UINT32 hi, UINT32 lo) { return ((UINT64)hi << 32) | lo; }    // Media Foundation stores pairs this way

    static bool startup(std::string& err) {
        static bool done = false, ok = false;
        if (done) { if (!ok) err = "Media Foundation failed to start"; return ok; }
        done = true;
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ok = SUCCEEDED(MFStartup(MF_VERSION));
        if (!ok) err = "Media Foundation failed to start";
        return ok;
    }
    static std::string hrText(const char* what, HRESULT hr) {
        char b[160];
        std::snprintf(b, sizeof b, "%s (error 0x%08lX)", what, (unsigned long)hr);
        return b;
    }
    bool fail(const char* what, HRESULT hr, std::string& err) {
        err = hrText(what, hr);
        if (writer_) { writer_->Release(); writer_ = nullptr; }
        return false;
    }
};
