#include "video.hpp"
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <cstring>

namespace vlab {

struct VideoPlayer::Impl {
    IMFSourceReader* reader = nullptr;
    IMFSample* pending = nullptr;
    double pendingTs = 0, clock = 0;
    LONG stride = 0;
    bool started = false;
};

VideoPlayer::VideoPlayer() {}
VideoPlayer::~VideoPlayer() { close(); }
bool VideoPlayer::isOpen() const { return p_ && p_->reader; }

void VideoPlayer::close() {
    if (!p_) return;
    if (p_->pending) p_->pending->Release();
    if (p_->reader) p_->reader->Release();
    if (p_->started) MFShutdown();
    delete p_;
    p_ = nullptr;
    w_ = h_ = 0;
    pix_.clear();
}

bool VideoPlayer::open(const std::string& path) {
    close();
    p_ = new Impl();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);                       // harmless if the thread already has COM
    if (FAILED(MFStartup(MF_VERSION))) { close(); return false; }
    p_->started = true;
    WCHAR wp[1024];
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wp, 1024);
    IMFAttributes* at = nullptr;
    MFCreateAttributes(&at, 1);
    at->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);          // let Windows convert whatever the file holds to RGB
    HRESULT hr = MFCreateSourceReaderFromURL(wp, at, &p_->reader);
    at->Release();
    if (FAILED(hr)) { close(); return false; }
    p_->reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    p_->reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
    IMFMediaType* mt = nullptr;
    MFCreateMediaType(&mt);
    mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    mt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    hr = p_->reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mt);
    mt->Release();
    if (FAILED(hr)) { close(); return false; }
    IMFMediaType* cur = nullptr;
    if (FAILED(p_->reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &cur))) { close(); return false; }
    UINT32 w = 0, h = 0;
    MFGetAttributeSize(cur, MF_MT_FRAME_SIZE, &w, &h);
    UINT32 st = 0;
    p_->stride = SUCCEEDED(cur->GetUINT32(MF_MT_DEFAULT_STRIDE, &st)) ? (LONG)(INT32)st : (LONG)(w * 4);
    cur->Release();
    if (w < 16 || h < 16 || w > 8192 || h > 8192) { close(); return false; }
    w_ = (int)w;
    h_ = (int)h;
    pix_.assign((size_t)w_ * h_, 0xFF000000u);
    return true;
}

bool VideoPlayer::advance(double dt) {
    if (!isOpen()) return false;
    Impl& d = *p_;
    d.clock += dt;
    bool changed = false;
    for (int guard = 0; guard < 2; ++guard) {
        if (!d.pending) {
            DWORD flags = 0;
            LONGLONG ts = 0;
            IMFSample* s = nullptr;
            HRESULT hr = d.reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &ts, &s);
            if (FAILED(hr)) return changed;
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {                         // loop: back to the start
                if (s) s->Release();
                PROPVARIANT v;
                PropVariantInit(&v);
                v.vt = VT_I8;
                v.hVal.QuadPart = 0;
                d.reader->SetCurrentPosition(GUID_NULL, v);
                PropVariantClear(&v);
                d.clock = 0;
                return changed;
            }
            if (!s) return changed;
            d.pending = s;
            d.pendingTs = (double)ts / 1e7;
        }
        if (d.pendingTs > d.clock) break;                                        // not due yet
        IMFMediaBuffer* buf = nullptr;
        if (SUCCEEDED(d.pending->ConvertToContiguousBuffer(&buf))) {
            BYTE* data = nullptr;
            DWORD maxLen = 0, curLen = 0;
            if (SUCCEEDED(buf->Lock(&data, &maxLen, &curLen))) {
                LONG rowBytes = d.stride < 0 ? -d.stride : d.stride;
                if (h_ > 0 && curLen % (DWORD)h_ == 0 && (LONG)(curLen / (DWORD)h_) >= w_ * 4) rowBytes = (LONG)(curLen / (DWORD)h_);   // trust the real buffer size over the stride attribute
                if ((size_t)curLen >= (size_t)rowBytes * (h_ - 1) + (size_t)w_ * 4) {
                    for (int y = 0; y < h_; ++y) {
                        const BYTE* src = d.stride < 0 ? data + (size_t)(h_ - 1 - y) * rowBytes : data + (size_t)y * rowBytes;
                        uint32_t* dst = &pix_[(size_t)y * w_];
                        std::memcpy(dst, src, (size_t)w_ * 4);
                        for (int x = 0; x < w_; ++x) dst[x] |= 0xFF000000u;
                    }
                    changed = true;
                }
                buf->Unlock();
            }
            buf->Release();
        }
        d.pending->Release();
        d.pending = nullptr;
        if (d.clock - d.pendingTs > 0.5) d.clock = d.pendingTs;                  // decoding fell behind: do not rush to catch up
    }
    return changed;
}

}  // namespace vlab
