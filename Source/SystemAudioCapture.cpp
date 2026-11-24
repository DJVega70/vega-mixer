#include "SystemAudioCapture.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #undef _WIN32_WINNT
 #define _WIN32_WINNT 0x0A00
 #include <windows.h>
 #include <objbase.h>
 #include <mmdeviceapi.h>
 #include <audioclient.h>
 #include <mmreg.h>
#endif

namespace
{
// Single-producer / single-consumer stereo ring buffer.
struct StereoRing
{
    std::vector<float> buf;
    size_t cap = 0;
    std::atomic<size_t> w { 0 }, r { 0 };

    void init (size_t frames) { buf.assign (frames * 2, 0.0f); cap = frames; w = 0; r = 0; }
    size_t available() const noexcept { return w.load (std::memory_order_acquire) - r.load (std::memory_order_acquire); }

    void push (float l, float rr) noexcept
    {
        const size_t wi = w.load (std::memory_order_relaxed);
        if (wi - r.load (std::memory_order_acquire) >= cap) return;   // full: drop
        const size_t i = (wi % cap) * 2;
        buf[i] = l; buf[i + 1] = rr;
        w.store (wi + 1, std::memory_order_release);
    }

    void pop (float& l, float& rr) noexcept
    {
        const size_t ri = r.load (std::memory_order_relaxed);
        const size_t i = (ri % cap) * 2;
        l = buf[i]; rr = buf[i + 1];
        r.store (ri + 1, std::memory_order_release);
    }

    void skip (size_t frames) noexcept { r.store (r.load (std::memory_order_relaxed) + frames, std::memory_order_release); }
};

// Linear resampler from the capture rate to the mixer rate.
struct Resampler
{
    double step = 1.0, pos = 0.0;
    float pl = 0.0f, pr = 0.0f;
    template <typename Out>
    void process (float l, float r, Out&& out)
    {
        if (std::abs (step - 1.0) < 1.0e-9) { out (l, r); return; }
        while (pos <= 1.0)
        {
            const float t = (float) pos;
            out (pl + (l - pl) * t, pr + (r - pr) * t);
            pos += step;
        }
        pos -= 1.0;
        pl = l; pr = r;
    }
};
} // namespace

struct SystemAudioCapture::Impl
{
    std::thread thread;
    std::atomic<bool> run { false }, running { false }, readable { false }, inRead { false };
    double sr = 48000.0;
    StereoRing ring;
    bool primed = false;
    mutable std::mutex statusLock;
    std::string statusText = "System audio off";

    void setStatus (std::string s) { std::lock_guard<std::mutex> g (statusLock); statusText = std::move (s); }
    void threadMain();
};

#ifdef _WIN32
namespace
{
// Same layout as AUDIOCLIENT_ACTIVATION_PARAMS (audioclientactivationparams.h, Windows 10 2004+).
struct VegaLoopbackParams   { DWORD targetProcessId; int processLoopbackMode; };
struct VegaActivationParams { int activationType; VegaLoopbackParams loopback; };

class ActivateHandler : public IActivateAudioInterfaceCompletionHandler, public IAgileObject
{
public:
    HANDLE done = CreateEventW (nullptr, TRUE, FALSE, nullptr);
    HRESULT result = E_FAIL;
    IAudioClient* client = nullptr;

    virtual ~ActivateHandler() { CloseHandle (done); }

    ULONG STDMETHODCALLTYPE AddRef() override  { return (ULONG) InterlockedIncrement (&refs); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const LONG n = InterlockedDecrement (&refs);
        if (n == 0) delete this;
        return (ULONG) n;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface (REFIID iid, void** out) override
    {
        if (iid == __uuidof (IUnknown) || iid == __uuidof (IActivateAudioInterfaceCompletionHandler))
            *out = static_cast<IActivateAudioInterfaceCompletionHandler*> (this);
        else if (iid == __uuidof (IAgileObject))
            *out = static_cast<IAgileObject*> (this);
        else { *out = nullptr; return E_NOINTERFACE; }
        AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ActivateCompleted (IActivateAudioInterfaceAsyncOperation* op) override
    {
        HRESULT hr = E_FAIL;
        IUnknown* unk = nullptr;
        if (SUCCEEDED (op->GetActivateResult (&hr, &unk)) && SUCCEEDED (hr) && unk != nullptr)
        {
            result = unk->QueryInterface (__uuidof (IAudioClient), (void**) &client);
            unk->Release();
        }
        else result = FAILED (hr) ? hr : E_FAIL;
        SetEvent (done);
        return S_OK;
    }

private:
    LONG refs = 1;
};

// Captures everything Windows plays EXCEPT this process (Windows 10 2004+).
IAudioClient* activateProcessLoopback()
{
    VegaActivationParams p {};
    p.activationType = 1;                                  // AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK
    p.loopback.targetProcessId = GetCurrentProcessId();
    p.loopback.processLoopbackMode = 1;                    // PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE

    PROPVARIANT pv;
    PropVariantInit (&pv);
    pv.vt = VT_BLOB;
    pv.blob.cbSize = sizeof (p);
    pv.blob.pBlobData = reinterpret_cast<BYTE*> (&p);

    auto* handler = new ActivateHandler();
    IActivateAudioInterfaceAsyncOperation* op = nullptr;
    IAudioClient* client = nullptr;
    const HRESULT hr = ActivateAudioInterfaceAsync (L"VAD\\Process_Loopback", __uuidof (IAudioClient), &pv, handler, &op);
    if (SUCCEEDED (hr) && WaitForSingleObject (handler->done, 3000) == WAIT_OBJECT_0 && SUCCEEDED (handler->result))
    {
        client = handler->client;
        handler->client = nullptr;
    }
    if (op != nullptr) op->Release();
    handler->Release();
    return client;
}

struct CaptureFormat
{
    bool isFloat = true;
    int bits = 32, channels = 2, blockAlign = 8;
    double rate = 48000.0;

    void from (const WAVEFORMATEX* wf)
    {
        channels = wf->nChannels;
        bits = wf->wBitsPerSample;
        blockAlign = wf->nBlockAlign;
        rate = (double) wf->nSamplesPerSec;
        isFloat = wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
        if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
            isFloat = reinterpret_cast<const WAVEFORMATEXTENSIBLE*> (wf)->SubFormat.Data1 == 3;   // KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
    }

    float sample (const BYTE* frame, int ch) const
    {
        const BYTE* p = frame + ch * (bits / 8);
        if (isFloat && bits == 32) { float f; std::memcpy (&f, p, 4); return f; }
        if (bits == 16) { int16_t v; std::memcpy (&v, p, 2); return (float) v / 32768.0f; }
        if (bits == 24) { const int v = (int) ((uint32_t) p[0] << 8 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 24) >> 8; return (float) v / 8388608.0f; }
        if (bits == 32) { int32_t v; std::memcpy (&v, p, 4); return (float) v / 2147483648.0f; }
        return 0.0f;
    }
};

WAVEFORMATEX makeFormat (double sr, bool asFloat)
{
    WAVEFORMATEX wf {};
    wf.wFormatTag = asFloat ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.nSamplesPerSec = (DWORD) sr;
    wf.wBitsPerSample = asFloat ? 32 : 16;
    wf.nBlockAlign = (WORD) (wf.nChannels * wf.wBitsPerSample / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    return wf;
}
} // namespace

void SystemAudioCapture::Impl::threadMain()
{
    const HRESULT coHr = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    const REFERENCE_TIME bufferDuration = 2000000;   // 200 ms
    const DWORD convertFlags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

    IAudioClient* client = nullptr;
    CaptureFormat fmt;
    bool excludesSelf = false, eventDriven = false;

    // 1) Process loopback that skips our own output.
    if ((client = activateProcessLoopback()) != nullptr)
    {
        for (bool asFloat : { true, false })
        {
            auto wf = makeFormat (sr, asFloat);
            if (SUCCEEDED (client->Initialize (AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK | convertFlags,
                                               bufferDuration, 0, &wf, nullptr)))
            {
                fmt.from (&wf);
                excludesSelf = eventDriven = true;
                break;
            }
        }
        if (! excludesSelf) { client->Release(); client = nullptr; }
    }

    // 2) Classic loopback of the default playback device.
    if (client == nullptr)
    {
        IMMDeviceEnumerator* en = nullptr;
        IMMDevice* dev = nullptr;
        if (SUCCEEDED (CoCreateInstance (__uuidof (MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof (IMMDeviceEnumerator), (void**) &en))
            && SUCCEEDED (en->GetDefaultAudioEndpoint (eRender, eConsole, &dev))
            && SUCCEEDED (dev->Activate (__uuidof (IAudioClient), CLSCTX_ALL, nullptr, (void**) &client)))
        {
            auto wf = makeFormat (sr, true);
            if (SUCCEEDED (client->Initialize (AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | convertFlags, bufferDuration, 0, &wf, nullptr)))
                fmt.from (&wf);
            else
            {
                WAVEFORMATEX* mix = nullptr;
                bool ok = false;
                if (SUCCEEDED (client->GetMixFormat (&mix)))
                {
                    ok = SUCCEEDED (client->Initialize (AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, bufferDuration, 0, mix, nullptr));
                    if (ok) fmt.from (mix);
                    CoTaskMemFree (mix);
                }
                if (! ok) { client->Release(); client = nullptr; }
            }
        }
        if (dev != nullptr) dev->Release();
        if (en != nullptr) en->Release();
    }

    IAudioCaptureClient* capture = nullptr;
    HANDLE evt = CreateEventW (nullptr, FALSE, FALSE, nullptr);
    if (client != nullptr && eventDriven) client->SetEventHandle (evt);
    if (client == nullptr || FAILED (client->GetService (__uuidof (IAudioCaptureClient), (void**) &capture)) || FAILED (client->Start()))
    {
        setStatus ("System audio unavailable on this PC");
        if (capture) capture->Release();
        if (client) client->Release();
        CloseHandle (evt);
        if (SUCCEEDED (coHr)) CoUninitialize();
        return;
    }

    setStatus (excludesSelf ? "System audio: capturing Windows playback"
                             : "System audio: capturing (includes this mixer - mute monitor M to avoid feedback)");
    running = true;

    Resampler rs;
    rs.step = fmt.rate / sr;

    while (run.load())
    {
        WaitForSingleObject (evt, eventDriven ? 20 : 5);
        UINT32 packet = 0;
        while (SUCCEEDED (capture->GetNextPacketSize (&packet)) && packet > 0)
        {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            if (FAILED (capture->GetBuffer (&data, &frames, &flags, nullptr, nullptr))) break;
            const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
            for (UINT32 f = 0; f < frames; ++f)
            {
                float l = 0.0f, r = 0.0f;
                if (! silent)
                {
                    const BYTE* frame = data + (size_t) f * (size_t) fmt.blockAlign;
                    l = fmt.sample (frame, 0);
                    r = fmt.channels > 1 ? fmt.sample (frame, 1) : l;
                }
                rs.process (l, r, [this] (float a, float b) { ring.push (a, b); });
            }
            capture->ReleaseBuffer (frames);
        }
    }

    client->Stop();
    capture->Release();
    client->Release();
    CloseHandle (evt);
    running = false;
    if (SUCCEEDED (coHr)) CoUninitialize();
}
#else
void SystemAudioCapture::Impl::threadMain()
{
    setStatus ("System audio capture is Windows only");
}
#endif

//==============================================================================
SystemAudioCapture::SystemAudioCapture() : impl (std::make_unique<Impl>()) {}
SystemAudioCapture::~SystemAudioCapture() { stop(); }

bool SystemAudioCapture::start (double sampleRate)
{
    if (impl->run.load() && std::abs (impl->sr - sampleRate) < 0.5) return true;
    stop();

    impl->readable = false;
    while (impl->inRead.load()) std::this_thread::yield();   // let the audio thread finish its read

    impl->sr = sampleRate;
    impl->ring.init ((size_t) (sampleRate * 2.0));
    impl->primed = false;
    impl->setStatus ("System audio: starting...");
    impl->run = true;
    impl->thread = std::thread ([this] { impl->threadMain(); });
    impl->readable = true;
    return true;
}

void SystemAudioCapture::stop()
{
    impl->readable = false;
    impl->run = false;
    if (impl->thread.joinable()) impl->thread.join();
    impl->running = false;
    impl->setStatus ("System audio off");
}

bool SystemAudioCapture::isRunning() const   { return impl->run.load(); }
double SystemAudioCapture::runningRate() const { return impl->sr; }

std::string SystemAudioCapture::status() const
{
    std::lock_guard<std::mutex> g (impl->statusLock);
    return impl->statusText;
}

void SystemAudioCapture::read (float* left, float* right, int n) noexcept
{
    impl->inRead = true;
    if (! impl->readable.load() || ! impl->running.load())
    {
        std::memset (left, 0, sizeof (float) * (size_t) n);
        std::memset (right, 0, sizeof (float) * (size_t) n);
        impl->inRead = false;
        return;
    }

    auto& ring = impl->ring;
    const size_t target = (size_t) (impl->sr * 0.05);    // ~50 ms cushion
    const size_t maxLag = (size_t) (impl->sr * 0.25);
    size_t avail = ring.available();

    if (! impl->primed)
    {
        if (avail < target + (size_t) n)
        {
            std::memset (left, 0, sizeof (float) * (size_t) n);
            std::memset (right, 0, sizeof (float) * (size_t) n);
            impl->inRead = false;
            return;
        }
        impl->primed = true;
    }
    if (avail > maxLag) { ring.skip (avail - target); avail = target; }   // catch up if the clocks drifted

    for (int i = 0; i < n; ++i)
    {
        if (avail > 0) { ring.pop (left[i], right[i]); --avail; }
        else { left[i] = right[i] = 0.0f; impl->primed = false; }
    }
    impl->inRead = false;
}
