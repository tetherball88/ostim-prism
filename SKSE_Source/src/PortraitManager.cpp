#include "PortraitManager.h"
#include "PrismaUIManager.h"
#include "Settings.h"
#include "UIUtil.h"
#include <Windows.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <thread>

#pragma comment(lib, "windowscodecs.lib")

namespace {
    // Phantasm 1.2.0 renders Framing::Portrait into a 1536x1536 offscreen (2x supersampled 768x768).
    // Requesting 1536x1536 avoids game-thread sRGB downscaling in FitStill and yields an exact 6x6 -> 1
    // linear-light box downscale to 256x256.
    constexpr uint32_t kRequestStillSize = 1536;
    constexpr uint32_t kOutputPortraitSize = 256;
    // turn=0;pitch=0 centers the face horizontally on the camera axis instead of Phantasm's 14-deg three-quarter default.
    // zoom=1.75 captures down through the chin/upper neck so we can shift the 0.88x crop window down (v=0.12..1.00),
    // trimming the helmet headroom at the top of Phantasm's frustum while keeping ~2x effective zoom (1.75 / 0.88 = 1.99).
    constexpr const char* kPortraitHints = "aim=face;zoom=1.75;turn=0;pitch=0";
    constexpr float kCropScale = 0.88f;
    constexpr float kCropCenterU = 0.50f;
    constexpr float kCropCenterV = 0.56f;
    constexpr auto kInitialSceneSettle = std::chrono::milliseconds(500);
    constexpr auto kEquipDebounce = std::chrono::milliseconds(350);
    constexpr auto kClassicRetryDelay = std::chrono::milliseconds(2000);
    constexpr auto kFailureRetryDelay = std::chrono::milliseconds(1500);
    constexpr int kMaxFailureRetries = 3;
    constexpr int kMaxClassicRetries = 2;

    template <class T>
    struct ComPtr {
        T* p = nullptr;
        ~ComPtr() {
            if (p) p->Release();
        }
        T** operator&() { return &p; }
        T* operator->() const { return p; }
    };

    const std::array<float, 256>& SrgbToLinearTable() {
        static const std::array<float, 256> table = []() {
            std::array<float, 256> t{};
            for (int i = 0; i < 256; ++i) {
                const float s = static_cast<float>(i) / 255.0f;
                t[static_cast<size_t>(i)] = (s <= 0.04045f)
                    ? (s / 12.92f)
                    : std::pow((s + 0.055f) / 1.055f, 2.4f);
            }
            return t;
        }();
        return table;
    }

    inline uint8_t LinearToSrgbByte(float l) noexcept {
        l = std::clamp(l, 0.0f, 1.0f);
        const float s = (l <= 0.0031308f)
            ? (l * 12.92f)
            : (1.055f * std::pow(l, 1.0f / 2.4f) - 0.055f);
        return static_cast<uint8_t>(std::clamp(std::lround(s * 255.0f), 0L, 255L));
    }

    // Downscales a centered/shifted crop of Phantasm's premultiplied sRGB RGBA8 buffer to outW x outH in linear light,
    // unpremultiplies alpha in linear light, and writes straight-alpha BGRA8 for WIC's PNG encoder.
    std::vector<uint8_t> DownscaleAndUnpremultiplyToBgra(
        const std::vector<uint8_t>& srcRgba,
        uint32_t srcW,
        uint32_t srcH,
        uint32_t outW,
        uint32_t outH)
    {
        if (srcW == 0 || srcH == 0 || outW == 0 || outH == 0) return {};
        if (srcRgba.size() < static_cast<size_t>(srcW) * srcH * 4) return {};

        const uint32_t minDim = std::min(srcW, srcH);
        const uint32_t cropSide = std::clamp<uint32_t>(
            static_cast<uint32_t>(std::lround(static_cast<float>(minDim) * kCropScale)),
            1u,
            minDim);
        const uint32_t cropX = static_cast<uint32_t>(std::clamp(
            std::lround(static_cast<float>(srcW) * kCropCenterU - static_cast<float>(cropSide) * 0.5f),
            0L,
            static_cast<long>(srcW - cropSide)));
        const uint32_t cropY = static_cast<uint32_t>(std::clamp(
            std::lround(static_cast<float>(srcH) * kCropCenterV - static_cast<float>(cropSide) * 0.5f),
            0L,
            static_cast<long>(srcH - cropSide)));

        const auto& lut = SrgbToLinearTable();
        std::vector<uint8_t> outBgra(static_cast<size_t>(outW) * outH * 4, 0);

        for (uint32_t oy = 0; oy < outH; ++oy) {
            const uint32_t y0 = cropY + (oy * cropSide) / outH;
            const uint32_t y1 = std::min(srcH, std::max(y0 + 1, cropY + ((oy + 1) * cropSide) / outH));
            for (uint32_t ox = 0; ox < outW; ++ox) {
                const uint32_t x0 = cropX + (ox * cropSide) / outW;
                const uint32_t x1 = std::min(srcW, std::max(x0 + 1, cropX + ((ox + 1) * cropSide) / outW));

                float sumR = 0.0f;
                float sumG = 0.0f;
                float sumB = 0.0f;
                float sumA = 0.0f;

                for (uint32_t sy = y0; sy < y1; ++sy) {
                    const uint8_t* row = srcRgba.data() + (static_cast<size_t>(sy) * srcW + x0) * 4;
                    for (uint32_t sx = x0; sx < x1; ++sx, row += 4) {
                        const uint8_t a = row[3];
                        if (a == 0) continue;
                        sumR += lut[row[0]];
                        sumG += lut[row[1]];
                        sumB += lut[row[2]];
                        sumA += static_cast<float>(a) * (1.0f / 255.0f);
                    }
                }

                const float count = static_cast<float>((y1 - y0) * (x1 - x0));
                const float meanA = sumA / count;
                const uint8_t outA = static_cast<uint8_t>(std::clamp(std::lround(meanA * 255.0f), 0L, 255L));
                if (outA == 0 || sumA <= 1e-6f) {
                    continue;
                }

                uint8_t* dst = outBgra.data() + (static_cast<size_t>(oy) * outW + ox) * 4;
                dst[0] = LinearToSrgbByte(sumB / sumA);
                dst[1] = LinearToSrgbByte(sumG / sumA);
                dst[2] = LinearToSrgbByte(sumR / sumA);
                dst[3] = outA;
            }
        }

        return outBgra;
    }

    std::vector<uint8_t> EncodeBgraToPng(const std::vector<uint8_t>& bgra, uint32_t w, uint32_t h) {
        const size_t bytes = static_cast<size_t>(w) * h * 4;
        if (w == 0 || h == 0 || bgra.size() < bytes) return {};

        ComPtr<IWICImagingFactory> factory;
        if (FAILED(::CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&factory)))) {
            return {};
        }

        ComPtr<IStream> stream;
        if (FAILED(::CreateStreamOnHGlobal(nullptr, TRUE, &stream))) return {};

        ComPtr<IWICBitmapEncoder> encoder;
        if (FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder))) return {};
        if (FAILED(encoder->Initialize(stream.p, WICBitmapEncoderNoCache))) return {};

        ComPtr<IWICBitmapFrameEncode> frame;
        if (FAILED(encoder->CreateNewFrame(&frame, nullptr))) return {};
        if (FAILED(frame->Initialize(nullptr))) return {};
        if (FAILED(frame->SetSize(w, h))) return {};

        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        if (FAILED(frame->SetPixelFormat(&format)) || format != GUID_WICPixelFormat32bppBGRA) return {};

        if (FAILED(frame->WritePixels(
                h,
                w * 4,
                static_cast<UINT>(bytes),
                const_cast<BYTE*>(bgra.data())))) {
            return {};
        }
        if (FAILED(frame->Commit()) || FAILED(encoder->Commit())) return {};

        STATSTG stat{};
        if (FAILED(stream->Stat(&stat, STATFLAG_NONAME))) return {};
        std::vector<uint8_t> out(static_cast<size_t>(stat.cbSize.QuadPart));
        LARGE_INTEGER zero{};
        if (FAILED(stream->Seek(zero, STREAM_SEEK_SET, nullptr))) return {};
        ULONG read = 0;
        if (FAILED(stream->Read(out.data(), static_cast<ULONG>(out.size()), &read)) || read != out.size()) {
            return {};
        }
        return out;
    }
}

void PortraitManager::Initialize() {
    if (!Settings::GetSingleton()->enablePortraits) {
        SKSE::log::info("PortraitManager: actor portraits disabled in OStimPrism.ini");
        return;
    }

    const HMODULE mod = ::GetModuleHandleW(L"Phantasm.dll");
    if (!mod) {
        SKSE::log::info("PortraitManager: Phantasm.dll not detected — using orgasm count in excitement circles");
        return;
    }

    auto* getApi = reinterpret_cast<PHANTASM_API::RequestPluginAPIFn>(
        ::GetProcAddress(mod, "RequestPluginAPI"));
    if (!getApi) {
        SKSE::log::warn("PortraitManager: Phantasm.dll does not export RequestPluginAPI");
        return;
    }

    const auto* api = static_cast<const PHANTASM_API::Interface1*>(getApi(PHANTASM_API::kInterface1));
    constexpr size_t kMinSize = offsetof(PHANTASM_API::Interface1, RequestStillHinted) + sizeof(void*);
    if (!api || api->size < kMinSize) {
        SKSE::log::warn("PortraitManager: Phantasm.dll is too old for portrait stills");
        return;
    }

    PHANTASM_API::ClientId cid = 0;
    const auto res = api->RegisterClient("ostimprism", &PortraitManager::OnPhantasmEvent, this, &cid);
    if (res != PHANTASM_API::Result::Ok || !cid) {
        SKSE::log::error("PortraitManager: Phantasm refused client registration ({})", static_cast<int>(res));
        return;
    }

    phantasm = api;
    clientId = cid;
    SKSE::log::info("PortraitManager: connected to Phantasm (version {}, client {})", api->pluginVersion, cid);
    if (api->pluginVersion < 10100) {
        SKSE::log::warn(
            "PortraitManager: Phantasm version {} is older than 1.2.0 (10100); 'aim=face;zoom=2' portrait hints require Phantasm 1.2.0+",
            api->pluginVersion);
    }
}

bool PortraitManager::IsAvailable() const {
    return phantasm != nullptr && clientId != 0 && Settings::GetSingleton()->enablePortraits;
}

void PortraitManager::OnGameLoad() {
    ++sessionGeneration;
    std::vector<uint32_t> toCancel;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [formId, state] : pending_) {
            if (state.inFlight && state.requestId != 0) {
                toCancel.push_back(state.requestId);
            }
        }
        pending_.clear();
        cache_.clear();
        sceneStartTime_ = {};
    }

    if (phantasm && clientId != 0 &&
        phantasm->size >= offsetof(PHANTASM_API::Interface1, CancelStill) + sizeof(void*)) {
        for (uint32_t reqId : toCancel) {
            phantasm->CancelStill(clientId, reqId);
        }
    }

    PrismaUIManager::GetSingleton()->ClearActorPortraits();
}

void PortraitManager::OnSceneChanged() {
    std::lock_guard<std::mutex> lock(mutex_);
    sceneStartTime_ = std::chrono::steady_clock::now();
}

void PortraitManager::SyncActors(const std::vector<OStimDataProvider::ActorData>& actors, bool forceResendToUI) {
    if (!IsAvailable()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    std::vector<std::pair<uint32_t, std::string>> portraitsToSend;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sceneStartTime_.time_since_epoch().count() == 0) {
            sceneStartTime_ = now;
        }

        const bool sceneSettled = (now - sceneStartTime_) >= kInitialSceneSettle;

        for (const auto& actor : actors) {
            const uint32_t formId = actor.formId;
            if (!formId) continue;

            const uint64_t equipSig = actor.equipSignature;
            auto& pend = pending_[formId];

            if (!pend.hasObservedEquip || pend.observedEquipSig != equipSig) {
                pend.hasObservedEquip = true;
                pend.observedEquipSig = equipSig;
                pend.equipStableSince = now;
                pend.failCount = 0;
                pend.retryAfter = {};
            }

            const bool equipSettled = (now - pend.equipStableSince) >= kEquipDebounce;

            auto cacheIt = cache_.find(formId);
            if (cacheIt != cache_.end()) {
                auto& entry = cacheIt->second;

                // Always send whatever portrait we currently have if the UI hasn't received it yet
                if (forceResendToUI || !entry.sentToUI) {
                    portraitsToSend.emplace_back(formId, entry.dataUrl);
                    entry.sentToUI = true;
                }

                if (entry.equipSignature != equipSig) {
                    // Equipment changed (e.g. OStim stripped armor/helmet): wait for equip to settle
                    if (sceneSettled && equipSettled && !pend.inFlight &&
                        pend.failCount < kMaxFailureRetries &&
                        now >= pend.retryAfter) {
                        RequestActorPortraitLocked(formId, equipSig, false);
                    }
                    continue;
                }

                if (sceneSettled && entry.classicPose && !pend.inFlight &&
                    entry.classicRetries < kMaxClassicRetries &&
                    (now - entry.capturedAt) >= kClassicRetryDelay &&
                    now >= pend.retryAfter) {
                    RequestActorPortraitLocked(formId, equipSig, true);
                }
                continue;
            }

            // No cached portrait yet for this actor
            if (sceneSettled && equipSettled && !pend.inFlight &&
                pend.failCount < kMaxFailureRetries &&
                now >= pend.retryAfter) {
                RequestActorPortraitLocked(formId, equipSig, false);
            }
        }
    }

    auto* ui = PrismaUIManager::GetSingleton();
    for (const auto& [formId, dataUrl] : portraitsToSend) {
        ui->SendActorPortrait(formId, dataUrl);
    }
}

void PortraitManager::RequestActorPortraitLocked(
    uint32_t formId,
    uint64_t equipSignature,
    bool isClassicRetry)
{
    if (!phantasm || !clientId) return;

    auto* ctx = new StillCallbackContext{
        .manager = this,
        .formId = formId,
        .equipSignature = equipSignature,
        .sessionGen = sessionGeneration.load(std::memory_order_relaxed),
        .isClassicRetry = isClassicRetry
    };

    uint32_t reqId = 0;
    const auto res = phantasm->RequestStillHinted(
        clientId,
        formId,
        kRequestStillSize,
        kRequestStillSize,
        PHANTASM_API::Framing::Portrait,
        kPortraitHints,
        &PortraitManager::OnStillDone,
        ctx,
        &reqId);

    if (res != PHANTASM_API::Result::Ok) {
        delete ctx;
        auto& pend = pending_[formId];
        ++pend.failCount;
        pend.retryAfter = std::chrono::steady_clock::now() + kFailureRetryDelay;
        SKSE::log::warn("PortraitManager: RequestStillHinted refused for {:08X} ({})",
            formId, static_cast<int>(res));
        return;
    }

    auto& pend = pending_[formId];
    pend.inFlight = true;
    pend.requestId = reqId;
    pend.inFlightEquipSig = equipSignature;
    pend.isClassicRetry = isClassicRetry;
    SKSE::log::info(
        "PortraitManager: requested portrait still for {:08X} (reqId={}, hints='{}', classicRetry={})",
        formId, reqId, kPortraitHints, isClassicRetry);
}

void PortraitManager::OnPhantasmEvent(const PHANTASM_API::Event* /*event*/, void* /*user*/) {
    // PortraitManager uses RequestStillHinted (stills only), which delivers outcomes via OnStillDone.
}

void PortraitManager::OnStillDone(const PHANTASM_API::Still* still, void* user) {
    std::unique_ptr<StillCallbackContext> ctx(static_cast<StillCallbackContext*>(user));
    if (!ctx || !ctx->manager) return;

    PortraitManager* self = ctx->manager;
    const uint32_t formId = ctx->formId;
    const uint64_t equipSig = ctx->equipSignature;
    const uint64_t sessionGen = ctx->sessionGen;
    const bool isClassicRetry = ctx->isClassicRetry;

    if (sessionGen != self->sessionGeneration.load(std::memory_order_relaxed)) {
        return;
    }

    if (!still || !still->ok || !still->rgba || still->width == 0 || still->height == 0) {
        const char* reason = (still && still->reason) ? still->reason : "unknown";
        SKSE::log::info("PortraitManager: portrait still for {:08X} not available ({})", formId, reason);
        std::lock_guard<std::mutex> lock(self->mutex_);
        auto& pend = self->pending_[formId];
        pend.inFlight = false;
        pend.requestId = 0;
        if (isClassicRetry) {
            if (auto it = self->cache_.find(formId); it != self->cache_.end()) {
                ++it->second.classicRetries;
                it->second.capturedAt = std::chrono::steady_clock::now();
            }
        } else {
            ++pend.failCount;
        }
        pend.retryAfter = std::chrono::steady_clock::now() + kFailureRetryDelay;
        return;
    }

    bool classicPose = false;
    if (still->size >= offsetof(PHANTASM_API::Still, classicPose) + sizeof(bool)) {
        classicPose = still->classicPose;
    }

    const uint32_t w = still->width;
    const uint32_t h = still->height;
    const size_t byteCount = static_cast<size_t>(w) * h * 4;
    std::vector<uint8_t> rgbaCopy(still->rgba, still->rgba + byteCount);

    self->PostWorkerJob([self, formId, equipSig, sessionGen, classicPose, isClassicRetry, w, h,
                         rgba = std::move(rgbaCopy)]() mutable {
        if (sessionGen != self->sessionGeneration.load(std::memory_order_relaxed)) {
            return;
        }

        const uint32_t outW = std::min(w, kOutputPortraitSize);
        const uint32_t outH = std::min(h, kOutputPortraitSize);
        auto bgra = DownscaleAndUnpremultiplyToBgra(rgba, w, h, outW, outH);
        auto png = EncodeBgraToPng(bgra, outW, outH);
        if (png.empty()) {
            SKSE::log::warn("PortraitManager: failed to encode PNG for {:08X}", formId);
            if (auto* taskInterface = SKSE::GetTaskInterface()) {
                taskInterface->AddTask([self, formId, sessionGen]() {
                    if (sessionGen != self->sessionGeneration.load(std::memory_order_relaxed)) {
                        return;
                    }
                    std::lock_guard<std::mutex> lock(self->mutex_);
                    auto& pend = self->pending_[formId];
                    pend.inFlight = false;
                    pend.requestId = 0;
                    ++pend.failCount;
                    pend.retryAfter = std::chrono::steady_clock::now() + kFailureRetryDelay;
                });
            }
            return;
        }

        std::string dataUrl = "data:image/png;base64," + UIUtil::base64Encode(png);
        if (auto* taskInterface = SKSE::GetTaskInterface()) {
            taskInterface->AddTask([self, formId, equipSig, sessionGen, classicPose, isClassicRetry,
                                    url = std::move(dataUrl)]() mutable {
                self->OnEncodedPortraitReady(
                    formId,
                    equipSig,
                    sessionGen,
                    classicPose,
                    isClassicRetry,
                    std::move(url));
            });
        }
    });
}

void PortraitManager::OnEncodedPortraitReady(
    uint32_t formId,
    uint64_t equipSignature,
    uint64_t sessionGen,
    bool classicPose,
    bool isClassicRetry,
    std::string dataUrl)
{
    if (sessionGen != sessionGeneration.load(std::memory_order_relaxed)) {
        return;
    }

    std::string urlToSend;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& pend = pending_[formId];
        pend.inFlight = false;
        pend.requestId = 0;
        pend.failCount = 0;

        auto& entry = cache_[formId];
        const bool sameEquip = (entry.equipSignature == equipSignature);
        const int prevClassicRetries = sameEquip ? entry.classicRetries : 0;

        entry.equipSignature = equipSignature;
        entry.dataUrl = std::move(dataUrl);
        entry.classicPose = classicPose;
        entry.sentToUI = true;
        entry.classicRetries = isClassicRetry ? (prevClassicRetries + 1) : prevClassicRetries;
        entry.capturedAt = std::chrono::steady_clock::now();

        urlToSend = entry.dataUrl;
    }

    SKSE::log::info(
        "PortraitManager: portrait ready for {:08X} ({} bytes, classicPose={})",
        formId, urlToSend.size(), classicPose);
    PrismaUIManager::GetSingleton()->SendActorPortrait(formId, urlToSend);
}

void PortraitManager::PostWorkerJob(std::function<void()> job) {
    std::lock_guard<std::mutex> lock(workerMutex_);
    if (!workerStarted_) {
        std::thread(&PortraitManager::WorkerLoop, this).detach();
        workerStarted_ = true;
    }
    workerJobs_.push_back(std::move(job));
    workerWake_.notify_one();
}

void PortraitManager::WorkerLoop(PortraitManager* self) {
    const HRESULT co = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(co)) {
        SKSE::log::warn("PortraitManager: CoInitializeEx returned {:08X}", static_cast<uint32_t>(co));
    }

    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(self->workerMutex_);
            self->workerWake_.wait(lock, [self]() { return !self->workerJobs_.empty(); });
            job = std::move(self->workerJobs_.front());
            self->workerJobs_.pop_front();
        }
        try {
            job();
        } catch (const std::exception& e) {
            SKSE::log::error("PortraitManager: worker exception: {}", e.what());
        } catch (...) {
            SKSE::log::error("PortraitManager: unknown worker exception");
        }
    }
}
