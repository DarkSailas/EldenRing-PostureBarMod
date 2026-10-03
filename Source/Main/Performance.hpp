#pragma once
#include "../Common.hpp"

namespace ER
{
    namespace Performance
    {
        // [Performance] section of PostureBarModConfig.ini
        inline bool frameTimeLog = false;
        inline bool cacheInputDevices = false;
        inline int inputDeviceRefreshSeconds = 3;
        inline bool sequentialFileRead = false;

        // Installs the hooks and starts the worker thread. MinHook must be initialized
        void Init();

        // Called from the Present hook for every presented frame
        void OnPresent();
    }
}
