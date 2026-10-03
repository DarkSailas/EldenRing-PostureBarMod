#include "Performance.hpp"
#include "../PostureBarMod.hpp"
#include "Logger.hpp"
#include <condition_variable>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

namespace ER
{
    namespace Performance
    {
        // -------------------------------------------------------------
        // Shared state of the worker thread
        // -------------------------------------------------------------
        static constexpr double kIntervalSeconds = 10.0;
        static constexpr size_t kMaxSamples = 16384;
        static constexpr size_t kMaxHitches = 256;
        static constexpr float kPauseMs = 1000.0f;   // longer gaps are loading screens or a minimized window
        static constexpr float kSlowEnumMs = 2.0f;

        struct Hitch
        {
            double time;
            float ms;
            float median;
        };

        struct FrameBatch
        {
            std::vector<float> ms;
            std::vector<Hitch> hitches;
            double endTime = 0.0;
            unsigned pauses = 0;

            void clear() { ms.clear(); hitches.clear(); pauses = 0; }
        };

        static std::mutex s_Mutex;
        static std::condition_variable s_Wake;
        static FrameBatch s_Pending;
        static bool s_PendingReady = false;
        static std::vector<std::string> s_Lines;
        static bool s_RefreshRequested = false;

        static LARGE_INTEGER s_QpcFreq = {};
        static LARGE_INTEGER s_QpcStart = {};

        static std::atomic<float> s_PrevMedian{ 0.0f };
        static std::atomic<unsigned> s_EnumCalls{ 0 };
        static std::atomic<unsigned> s_EnumReal{ 0 };
        static std::atomic<unsigned> s_EnumRealMicros{ 0 };
        static std::atomic<unsigned> s_FileHints{ 0 };

        static double Now()
        {
            LARGE_INTEGER t;
            QueryPerformanceCounter(&t);
            return double(t.QuadPart - s_QpcStart.QuadPart) / double(s_QpcFreq.QuadPart);
        }

        static void QueueLine(const char* text)
        {
            char buf[320];
            snprintf(buf, sizeof(buf), "[%9.2fs] %s\n", Now(), text);
            std::lock_guard<std::mutex> lock(s_Mutex);
            s_Lines.emplace_back(buf);
        }

        // -------------------------------------------------------------
        // Frame time meter. Runs on the render thread, so it only stores samples;
        // statistics and file writes happen on the worker thread
        // -------------------------------------------------------------
        static FrameBatch s_Current;
        static LONGLONG s_LastPresent = 0;
        static double s_IntervalStart = 0.0;

        void OnPresent()
        {
            if (!frameTimeLog || s_QpcFreq.QuadPart == 0)
                return;

            LARGE_INTEGER t;
            QueryPerformanceCounter(&t);
            const LONGLONG last = s_LastPresent;
            s_LastPresent = t.QuadPart;
            if (last == 0)
                return;

            const float ms = float(double(t.QuadPart - last) * 1000.0 / double(s_QpcFreq.QuadPart));
            const double now = double(t.QuadPart - s_QpcStart.QuadPart) / double(s_QpcFreq.QuadPart);

            if (ms > kPauseMs)
            {
                s_Current.pauses++;
            }
            else
            {
                s_Current.ms.push_back(ms);
                const float median = s_PrevMedian.load(std::memory_order_relaxed);
                if (median > 0.0f && ms > std::max(median * 2.0f, median + 4.0f) && s_Current.hitches.size() < kMaxHitches)
                    s_Current.hitches.push_back({ now, ms, median });
            }

            if (now - s_IntervalStart < kIntervalSeconds && s_Current.ms.size() < kMaxSamples)
                return;

            s_IntervalStart = now;
            s_Current.endTime = now;
            {
                std::lock_guard<std::mutex> lock(s_Mutex);
                if (!s_PendingReady)
                {
                    // Both batches keep their capacity, so there is no allocation here
                    std::swap(s_Current, s_Pending);
                    s_PendingReady = true;
                }
            }
            s_Current.clear();
            s_Wake.notify_one();
        }

        static void WriteBatch(FILE* file, FrameBatch& batch)
        {
            for (const Hitch& h : batch.hitches)
                fprintf(file, "[%9.2fs] hitch %.1f ms (median %.1f ms)\n", h.time, h.ms, h.median);

            const unsigned enumCalls = s_EnumCalls.exchange(0);
            const unsigned enumReal = s_EnumReal.exchange(0);
            const unsigned enumMicros = s_EnumRealMicros.exchange(0);

            std::vector<float>& ms = batch.ms;
            if (ms.empty())
            {
                fprintf(file, "[%9.2fs] frames=0 pauses=%u\n", batch.endTime, batch.pauses);
                return;
            }

            std::sort(ms.begin(), ms.end());
            const size_t n = ms.size();
            double sum = 0.0;
            for (float v : ms)
                sum += v;
            const double avg = sum / double(n);
            const float median = ms[n / 2];
            const float p99 = ms[std::min(n - 1, size_t(double(n) * 0.99))];

            // 1% low: average FPS over the slowest 1% of frames
            const size_t worst = std::max<size_t>(1, n / 100);
            double worstSum = 0.0;
            for (size_t i = n - worst; i < n; i++)
                worstSum += ms[i];
            const double low = 1000.0 / (worstSum / double(worst));

            s_PrevMedian.store(median, std::memory_order_relaxed);

            fprintf(file,
                "[%9.2fs] frames=%zu avg=%.2f ms (%.1f fps) median=%.2f p99=%.2f max=%.2f 1%%low=%.1f fps hitches=%zu pauses=%u enum=%u (real %u, %.1f ms)\n",
                batch.endTime, n, avg, 1000.0 / avg, median, p99, ms[n - 1], low,
                batch.hitches.size(), batch.pauses, enumCalls, enumReal, double(enumMicros) / 1000.0);
        }

        // -------------------------------------------------------------
        // DirectInput device list cache. IDirectInput8::EnumDevices walks every HID
        // device and can block the calling thread for tens of milliseconds. The game
        // gets the list from the last enumeration; the list is refreshed on the
        // worker thread through a separate IDirectInput8 instance
        // -------------------------------------------------------------
        typedef HRESULT(WINAPI* FnEnumDevicesW)(IDirectInput8W* self, DWORD dwDevType, LPDIENUMDEVICESCALLBACKW lpCallback, LPVOID pvRef, DWORD dwFlags);
        typedef HRESULT(WINAPI* FnEnumDevicesA)(IDirectInput8A* self, DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags);
        static FnEnumDevicesW oEnumDevicesW = nullptr;
        static FnEnumDevicesA oEnumDevicesA = nullptr;

        template <typename Inst>
        struct EnumCache
        {
            struct Entry
            {
                DWORD devType;
                DWORD flags;
                std::vector<Inst> devices;
                double stamp;
                bool refreshing;
            };

            // Guarded by s_Mutex
            std::vector<Entry> entries;
        };

        static EnumCache<DIDEVICEINSTANCEW> s_CacheW;
        static EnumCache<DIDEVICEINSTANCEA> s_CacheA;

        template <typename Inst>
        static BOOL CALLBACK CollectDevice(const Inst* instance, LPVOID ref)
        {
            if (instance)
                static_cast<std::vector<Inst>*>(ref)->push_back(*instance);
            return DIENUM_CONTINUE;
        }

        template <typename DI, typename Inst, typename Fn>
        static HRESULT TimedEnum(Fn original, DI* self, DWORD devType, BOOL(CALLBACK* callback)(const Inst*, LPVOID), LPVOID ref, DWORD flags, bool fromGame)
        {
            const double start = Now();
            const HRESULT hr = original(self, devType, callback, ref, flags);
            const double ms = (Now() - start) * 1000.0;

            s_EnumReal++;
            s_EnumRealMicros += unsigned(ms * 1000.0);
            if (fromGame && ms >= kSlowEnumMs)
            {
                char buf[160];
                snprintf(buf, sizeof(buf), "EnumDevices(type=0x%lX, flags=0x%lX) took %.1f ms on the game thread", devType, flags, ms);
                QueueLine(buf);
            }
            return hr;
        }

        template <typename DI, typename Inst, typename Fn>
        static HRESULT EnumCached(EnumCache<Inst>& cache, Fn original, DI* self, DWORD devType, BOOL(CALLBACK* callback)(const Inst*, LPVOID), LPVOID ref, DWORD flags)
        {
            s_EnumCalls++;
            if (!callback)
                return original(self, devType, callback, ref, flags);
            if (!cacheInputDevices)
                return TimedEnum(original, self, devType, callback, ref, flags, true);

            std::vector<Inst> devices;
            bool hit = false;
            bool wake = false;
            {
                std::lock_guard<std::mutex> lock(s_Mutex);
                const double now = Now();
                for (auto& entry : cache.entries)
                {
                    if (entry.devType != devType || entry.flags != flags)
                        continue;
                    devices = entry.devices;
                    hit = true;
                    if (!entry.refreshing && now - entry.stamp >= double(inputDeviceRefreshSeconds))
                    {
                        entry.refreshing = true;
                        s_RefreshRequested = true;
                        wake = true;
                    }
                    break;
                }
            }

            if (!hit)
            {
                const HRESULT hr = TimedEnum(original, self, devType, &CollectDevice<Inst>, &devices, flags, true);
                if (FAILED(hr))
                    return hr;
                std::lock_guard<std::mutex> lock(s_Mutex);
                cache.entries.push_back({ devType, flags, devices, Now(), false });
            }

            if (wake)
                s_Wake.notify_one();

            for (const Inst& device : devices)
            {
                if (callback(&device, ref) == DIENUM_STOP)
                    break;
            }
            return DI_OK;
        }

        static HRESULT WINAPI Hooked_EnumDevicesW(IDirectInput8W* self, DWORD dwDevType, LPDIENUMDEVICESCALLBACKW lpCallback, LPVOID pvRef, DWORD dwFlags)
        {
            return EnumCached(s_CacheW, oEnumDevicesW, self, dwDevType, lpCallback, pvRef, dwFlags);
        }

        static HRESULT WINAPI Hooked_EnumDevicesA(IDirectInput8A* self, DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags)
        {
            return EnumCached(s_CacheA, oEnumDevicesA, self, dwDevType, lpCallback, pvRef, dwFlags);
        }

        // Worker thread: re-enumerates the entries marked for refresh with its own instance
        template <typename DI, typename Inst, typename Fn>
        static void RefreshCache(EnumCache<Inst>& cache, Fn original, DI* own)
        {
            if (!own || !original)
            {
                // No instance to refresh with: drop the stale lists, the game's next call enumerates itself
                std::lock_guard<std::mutex> lock(s_Mutex);
                std::erase_if(cache.entries, [](const auto& entry) { return entry.refreshing; });
                return;
            }

            std::vector<std::pair<DWORD, DWORD>> keys;
            {
                std::lock_guard<std::mutex> lock(s_Mutex);
                for (auto& entry : cache.entries)
                    if (entry.refreshing)
                        keys.emplace_back(entry.devType, entry.flags);
            }

            for (auto& [devType, flags] : keys)
            {
                std::vector<Inst> devices;
                const HRESULT hr = TimedEnum(original, own, devType, &CollectDevice<Inst>, &devices, flags, false);

                std::lock_guard<std::mutex> lock(s_Mutex);
                for (auto& entry : cache.entries)
                {
                    if (entry.devType != devType || entry.flags != flags)
                        continue;
                    if (SUCCEEDED(hr))
                        entry.devices = std::move(devices);
                    entry.stamp = Now();
                    entry.refreshing = false;
                    break;
                }
            }
        }

        static void InstallEnumDevicesHooks()
        {
            // The vtable is shared by all instances, so a temporary one gives the address
            IDirectInput8W* diW = nullptr;
            if (SUCCEEDED(DirectInput8Create(GetModuleHandleW(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&diW, NULL)) && diW)
            {
                void* target = (*(void***)diW)[4];
                if (MH_CreateHook(target, (void*)&Hooked_EnumDevicesW, (void**)&oEnumDevicesW) == MH_OK && MH_EnableHook(target) == MH_OK)
                    Logger::log("Performance: IDirectInput8W::EnumDevices hooked");
                else
                    Logger::log("Performance: IDirectInput8W::EnumDevices hook failed", LogLevel::Warning);
                diW->Release();
            }

            IDirectInput8A* diA = nullptr;
            if (SUCCEEDED(DirectInput8Create(GetModuleHandleW(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8A, (void**)&diA, NULL)) && diA)
            {
                void* target = (*(void***)diA)[4];
                if (MH_CreateHook(target, (void*)&Hooked_EnumDevicesA, (void**)&oEnumDevicesA) == MH_OK && MH_EnableHook(target) == MH_OK)
                    Logger::log("Performance: IDirectInput8A::EnumDevices hooked");
                else
                    Logger::log("Performance: IDirectInput8A::EnumDevices hook failed", LogLevel::Warning);
                diA->Release();
            }
        }

        // -------------------------------------------------------------
        // Read-ahead hint for loose .dcx files. They are read from start to end and
        // unpacked in memory, so FILE_FLAG_SEQUENTIAL_SCAN matches the access pattern
        // -------------------------------------------------------------
        typedef HANDLE(WINAPI* FnCreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
        static FnCreateFileW oCreateFileW = nullptr;

        static bool IsLooseAsset(LPCWSTR path)
        {
            const size_t length = wcslen(path);
            if (length < 4)
                return false;
            const wchar_t* ext = path + length - 4;
            return ext[0] == L'.' && (ext[1] | 0x20) == L'd' && (ext[2] | 0x20) == L'c' && (ext[3] | 0x20) == L'x';
        }

        static HANDLE WINAPI Hooked_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
        {
            const DWORD excluded = FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_SEQUENTIAL_SCAN;
            if (lpFileName
                && dwCreationDisposition == OPEN_EXISTING
                && (dwDesiredAccess & GENERIC_READ) && !(dwDesiredAccess & GENERIC_WRITE)
                && !(dwFlagsAndAttributes & excluded)
                && IsLooseAsset(lpFileName))
            {
                dwFlagsAndAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;
                s_FileHints.fetch_add(1, std::memory_order_relaxed);
            }
            return oCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
        }

        static void InstallFileHook()
        {
            // kernel32!CreateFileW only forwards to kernelbase, hook the implementation
            LPVOID target = nullptr;
            MH_STATUS status = MH_CreateHookApiEx(L"kernelbase.dll", "CreateFileW", (void*)&Hooked_CreateFileW, (void**)&oCreateFileW, &target);
            if (status != MH_OK)
                status = MH_CreateHookApiEx(L"kernel32.dll", "CreateFileW", (void*)&Hooked_CreateFileW, (void**)&oCreateFileW, &target);

            if (status == MH_OK && MH_EnableHook(target) == MH_OK)
                Logger::log("Performance: CreateFileW hooked");
            else
                Logger::log("Performance: CreateFileW hook failed", LogLevel::Warning);
        }

        // -------------------------------------------------------------
        // Worker thread
        // -------------------------------------------------------------
        static void WorkerThread()
        {
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

            FILE* file = fopen((dllPath + "PostureBarMod_performance.log").c_str(), "w");
            if (file)
            {
                fprintf(file, "PostureBarMod performance log. FrameTimeLog=%d CacheInputDevices=%d InputDeviceRefreshSeconds=%d SequentialFileRead=%d\n",
                    frameTimeLog, cacheInputDevices, inputDeviceRefreshSeconds, sequentialFileRead);
                fflush(file);
            }

            IDirectInput8W* ownW = nullptr;
            IDirectInput8A* ownA = nullptr;
            if (cacheInputDevices)
            {
                DirectInput8Create(GetModuleHandleW(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&ownW, NULL);
                DirectInput8Create(GetModuleHandleW(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8A, (void**)&ownA, NULL);
            }

            FrameBatch batch;
            batch.ms.reserve(kMaxSamples);
            batch.hitches.reserve(kMaxHitches);
            std::vector<std::string> lines;
            unsigned reportedHints = 0;

            while (g_Running)
            {
                bool haveBatch = false;
                bool refresh = false;
                {
                    std::unique_lock<std::mutex> lock(s_Mutex);
                    s_Wake.wait_for(lock, std::chrono::seconds(1), [] { return s_PendingReady || s_RefreshRequested || !s_Lines.empty(); });
                    if (s_PendingReady)
                    {
                        std::swap(batch, s_Pending);
                        s_Pending.clear();
                        s_PendingReady = false;
                        haveBatch = true;
                    }
                    refresh = s_RefreshRequested;
                    s_RefreshRequested = false;
                    lines.swap(s_Lines);
                }

                if (refresh)
                {
                    RefreshCache(s_CacheW, oEnumDevicesW, ownW);
                    RefreshCache(s_CacheA, oEnumDevicesA, ownA);
                }

                if (!file)
                {
                    lines.clear();
                    continue;
                }

                for (const std::string& line : lines)
                    fputs(line.c_str(), file);

                if (haveBatch)
                    WriteBatch(file, batch);

                const unsigned hints = s_FileHints.load(std::memory_order_relaxed);
                if (hints != reportedHints && (haveBatch || !frameTimeLog))
                {
                    fprintf(file, "[%9.2fs] sequential read hint applied to %u files since start\n", Now(), hints);
                    reportedHints = hints;
                }

                if (haveBatch || !lines.empty())
                    fflush(file);
                lines.clear();
            }

            if (ownW) ownW->Release();
            if (ownA) ownA->Release();
            if (file) fclose(file);
        }

        void Init()
        {
            if (!frameTimeLog && !cacheInputDevices && !sequentialFileRead)
                return;

            QueryPerformanceFrequency(&s_QpcFreq);
            QueryPerformanceCounter(&s_QpcStart);

            if (inputDeviceRefreshSeconds < 1)
                inputDeviceRefreshSeconds = 1;

            s_Current.ms.reserve(kMaxSamples);
            s_Current.hitches.reserve(kMaxHitches);
            s_Pending.ms.reserve(kMaxSamples);
            s_Pending.hitches.reserve(kMaxHitches);

            // With FrameTimeLog alone the hook only measures how long the game's own enumeration takes
            if (cacheInputDevices || frameTimeLog)
                InstallEnumDevicesHooks();
            if (sequentialFileRead)
                InstallFileHook();

            std::thread(WorkerThread).detach();
            Logger::log("Performance: worker started");
        }
    }
}
