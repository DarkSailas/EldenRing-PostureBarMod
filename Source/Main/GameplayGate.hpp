#pragma once

namespace ER
{
    // Tells whether the game is in plain play: no menu, no popup, no loading or fade screen.
    namespace GameplayGate
    {
        inline bool hideInMenus = true;
        inline bool hideOnLoadingScreens = true;

        // Call once per rendered frame from the render thread.
        bool visible();
    }
}
