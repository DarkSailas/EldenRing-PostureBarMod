#include "VisualAtmosphereUI.hpp"
#include <windows.h>
#include <cstdio>

bool g_ShowVAMenu = false;

typedef VA::AtmosphereConfig* (*FnVA_GetConfig)();
typedef void (*FnVA_SaveConfig)();
typedef void (*FnVA_ApplyPreset)(int id);

static FnVA_GetConfig pfnVA_GetConfig = nullptr;
static FnVA_SaveConfig pfnVA_SaveConfig = nullptr;
static FnVA_ApplyPreset pfnVA_ApplyPreset = nullptr;
static bool g_VAFunctionsLoaded = false;

static void EnsureVALoaded()
{
    if (g_VAFunctionsLoaded) return;
    HMODULE hMod = GetModuleHandleA("VisualAtmosphere.dll");
    if (!hMod) hMod = LoadLibraryA("VisualAtmosphere.dll");
    if (hMod)
    {
        pfnVA_GetConfig = (FnVA_GetConfig)GetProcAddress(hMod, "VA_GetConfig");
        pfnVA_SaveConfig = (FnVA_SaveConfig)GetProcAddress(hMod, "VA_SaveConfig");
        pfnVA_ApplyPreset = (FnVA_ApplyPreset)GetProcAddress(hMod, "VA_ApplyPreset");
        if (pfnVA_GetConfig && pfnVA_SaveConfig && pfnVA_ApplyPreset)
            g_VAFunctionsLoaded = true;
    }
}

bool IsVisualAtmosphereAvailable()
{
    EnsureVALoaded();
    return g_VAFunctionsLoaded;
}

void DrawVisualAtmosphereMenu()
{
    EnsureVALoaded();
    if (!g_VAFunctionsLoaded || !pfnVA_GetConfig) return;

    VA::AtmosphereConfig* cfg = pfnVA_GetConfig();
    if (!cfg) return;

    ImGui::SetNextWindowSize(ImVec2(450, 520), ImGuiCond_FirstUseEver);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));

    if (ImGui::Begin("Visual Atmosphere  |  Dark Fantasy Engine v2.3", &g_ShowVAMenu, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::GetIO().MouseDrawCursor = true;
        if (!g_ShowVAMenu)
        {
            ImGui::GetIO().MouseDrawCursor = false;
            while (ShowCursor(FALSE) >= 0);
        }

        // 1. Enable Checkbox
        bool enabled = cfg->enableAtmosphere;
        if (ImGui::Checkbox("Enable Visual Atmosphere", &enabled))
        {
            cfg->enableAtmosphere = enabled;
            if (pfnVA_SaveConfig) pfnVA_SaveConfig();
        }

        ImGui::Separator();

        // 2. Preset Selection
        const char* presets[] = {
            "0. Bloodborne / Yharnam Noir",
            "1. Dark Souls 3 / Lothric Ashen",
            "2. Demon's Souls / Boletaria Deep Cold",
            "3. Cold Slate (Cinematic)",
            "4. Custom"
        };
        int currentPreset = cfg->activePreset;
        if (ImGui::Combo("Preset", &currentPreset, presets, IM_ARRAYSIZE(presets)))
        {
            if (pfnVA_ApplyPreset) pfnVA_ApplyPreset(currentPreset);
            if (pfnVA_SaveConfig) pfnVA_SaveConfig();
        }

        ImGui::Separator();

        // 3. RGB Multipliers
        ImGui::TextColored(ImVec4(0.88f, 0.76f, 0.48f, 1.0f), "RGB Channel Multipliers");
        bool changed = false;
        if (ImGui::SliderFloat("Red", &cfg->scaleR, 0.50f, 1.50f, "%.3f")) changed = true;
        if (ImGui::SliderFloat("Green", &cfg->scaleG, 0.50f, 1.50f, "%.3f")) changed = true;
        if (ImGui::SliderFloat("Blue", &cfg->scaleB, 0.50f, 1.50f, "%.3f")) changed = true;

        ImGui::Separator();

        // 4. Tonal & Post-Processing Grading
        ImGui::TextColored(ImVec4(0.88f, 0.76f, 0.48f, 1.0f), "Tonal & Post-Processing Grading");
        if (ImGui::SliderFloat("Saturation", &cfg->saturation, 0.00f, 2.00f, "%.2f")) changed = true;
        if (ImGui::SliderFloat("Contrast", &cfg->contrast, 0.50f, 2.00f, "%.2f")) changed = true;
        if (ImGui::SliderFloat("Shadow Depth", &cfg->shadowDepth, 0.00f, 0.15f, "%.3f")) changed = true;
        if (ImGui::SliderFloat("Vignette", &cfg->vignetteIntensity, 0.00f, 0.50f, "%.2f")) changed = true;

        if (changed && cfg->activePreset != 4)
        {
            cfg->activePreset = 4; // Switch to Custom
        }

        ImGui::Separator();

        // 5. Action Buttons
        float btnWidth = 200.0f;
        if (ImGui::Button("Save as Custom", ImVec2(btnWidth, 26)))
        {
            cfg->customScaleR = cfg->scaleR;
            cfg->customScaleG = cfg->scaleG;
            cfg->customScaleB = cfg->scaleB;
            cfg->customSaturation = cfg->saturation;
            cfg->customContrast = cfg->contrast;
            cfg->customShadowDepth = cfg->shadowDepth;
            cfg->customVignetteIntensity = cfg->vignetteIntensity;
            cfg->activePreset = 4;
            if (pfnVA_SaveConfig) pfnVA_SaveConfig();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save to INI", ImVec2(btnWidth, 26)))
        {
            if (pfnVA_SaveConfig) pfnVA_SaveConfig();
        }

        if (ImGui::Button("Reset to Bloodborne", ImVec2(btnWidth, 26)))
        {
            if (pfnVA_ApplyPreset) pfnVA_ApplyPreset(0);
            if (pfnVA_SaveConfig) pfnVA_SaveConfig();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close (F5)", ImVec2(btnWidth, 26)))
        {
            g_ShowVAMenu = false;
            ImGui::GetIO().MouseDrawCursor = false;
            while (ShowCursor(FALSE) >= 0);
        }

        ImGui::Separator();
        ImGui::TextDisabled("Toggle Hotkey: F5  |  Close: ESC or F5");
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
}
