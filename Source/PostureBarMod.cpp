#include "PostureBarMod.hpp"
#include "Ini/ini.h"
#include "Main/Logger.hpp"
#include "Main/D3DRenderer.hpp"
#include "Main/Hooking.hpp"
#include "Main/PostureBarUI.hpp"

using namespace ER;

bool loadIni()
{
    try    
    {
        using namespace mINI;
        auto safe_stof = [](const std::string& str, float default_val = 0.0f) -> float {
            if (str.empty()) return default_val;
            try {
                std::string s = str;
                while (!s.empty() && (s.back() == 'f' || s.back() == 'F' || s.back() == ' ' || s.back() == '\t'))
                    s.pop_back();
                size_t start = s.find_first_not_of(" \t");
                if (start == std::string::npos) return default_val;
                return std::stof(s.substr(start));
            } catch (...) {
                return default_val;
            }
        };

        auto safe_stoi = [](const std::string& str, int default_val = 0) -> int {
            if (str.empty()) return default_val;
            try {
                std::string s = str;
                size_t start = s.find_first_not_of(" \t");
                if (start == std::string::npos) return default_val;
                return std::stoi(s.substr(start));
            } catch (...) {
                return default_val;
            }
        };
        INIFile config(dllPath + "PostureBarModConfig.ini");
        INIStructure ini;

        if (!config.read(ini))
            throw std::runtime_error("Failed to read PostureBarModConfig.ini in mods folder");

        //-----------------------------------------------------------------------------------
        //                                        General
        //-----------------------------------------------------------------------------------
        ScreenParams::inGameCoordSizeX = safe_stof(ini["General"].get("InGameScreenCoordWidth"));
        ScreenParams::inGameCoordSizeY = safe_stof(ini["General"].get("InGameScreenCoordHeight"));
        ScreenParams::posX = safe_stof(ini["General"].get("ScreenPositionX"));
        ScreenParams::posY = safe_stof(ini["General"].get("ScreenPositionY"));
        ScreenParams::gameToViewportScaling = safe_stof(ini["General"].get("GameToScreenScaling"));
        ScreenParams::autoPositionSetup = ini["General"].get("AutoPositionSetup") == "true";
        ScreenParams::autoGameToViewportScaling = ini["General"].get("AutoGameToScreenScaling") == "true";

        //-----------------------------------------------------------------------------------
        //                                        Textures
        //-----------------------------------------------------------------------------------
        TextureData::useTextures = ini["Textures"].get("UseTextures") == "true";
        TextureFileData::bossBarFile = dllPath + ini["Textures"].get("BossBarFillFile");
        TextureFileData::bossBorderFile = dllPath + ini["Textures"].get("BossBarBorderFile");
        TextureFileData::entityBarFile = dllPath + ini["Textures"].get("EntityBarFillFile");
        TextureFileData::entityBorderFile = dllPath + ini["Textures"].get("EntityBarBorderFile");
        TextureFileData::circleBorderFile = dllPath + ini["Textures"].get("CircleBorderFile");
        TextureData::bossOffset = FillTextureOffset{
            { safe_stof(ini["Textures"].get("BossFillTopLeftOffsetX")), safe_stof(ini["Textures"].get("BossFillTopLeftOffsetY")) },
            { safe_stof(ini["Textures"].get("BossFillBotRightOffsetX")), safe_stof(ini["Textures"].get("BossFillBotRightOffsetY")) }
        };
        TextureData::entityOffset = FillTextureOffset{
            { safe_stof(ini["Textures"].get("EntityFillTopLeftOffsetX")), safe_stof(ini["Textures"].get("EntityFillTopLeftOffsetY")) },
            { safe_stof(ini["Textures"].get("EntityFillBotRightOffsetX")), safe_stof(ini["Textures"].get("EntityFillBotRightOffsetY")) }
        };
        TextureData::circleOffset = { safe_stof(ini["Textures"].get("CircleFillOffsetX")), safe_stof(ini["Textures"].get("CircleFillOffsetY")) };

        //-----------------------------------------------------------------------------------
        //                                        Style
        //-----------------------------------------------------------------------------------
        BarStyle::statusBarShape = EBarShapeType(safe_stoi(ini["Style"].get("StatusBarShape")));
        assert((int)BarStyle::statusBarShape >= 0 && BarStyle::statusBarShape < EBarShapeType::Last);
        BarStyle::fillAlignment = EFillAlignment(safe_stoi(ini["Style"].get("FillAlignment")));
        assert((int)BarStyle::fillAlignment >= 0 && BarStyle::fillAlignment < EFillAlignment::Last);
        BarStyle::fillType = EFillType(safe_stoi(ini["Style"].get("FillType")));
        assert((int)BarStyle::fillType >= 0 && BarStyle::fillType < EFillType::Last);
        BarStyle::fillResizeType = EFillResizeType(safe_stoi(ini["Style"].get("FillResizeType")));
        assert((int)BarStyle::fillResizeType >= 0 && BarStyle::fillResizeType < EFillResizeType::Last);

        auto&& staggerColorMax = splitString(ini["Style"].get("StaggerColorMax"), ",");
        assert(staggerColorMax.size() == 4);
        BarStyle::staggerMaxColor = ImVec4(safe_stof(staggerColorMax[0]), safe_stof(staggerColorMax[1]), safe_stof(staggerColorMax[2]), safe_stof(staggerColorMax[3]));

        auto&& staggerColorMin = splitString(ini["Style"].get("StaggerColorMin"), ",");
        assert(staggerColorMin.size() == 4);
        BarStyle::staggerMinColor = ImVec4(safe_stof(staggerColorMin[0]), safe_stof(staggerColorMin[1]), safe_stof(staggerColorMin[2]), safe_stof(staggerColorMin[3]));

        auto&& staminaColorMax = splitString(ini["Style"].get("StaminaColorMax"), ",");
        assert(staminaColorMax.size() == 4);
        BarStyle::staminaMaxColor = ImVec4(safe_stof(staminaColorMax[0]), safe_stof(staminaColorMax[1]), safe_stof(staminaColorMax[2]), safe_stof(staminaColorMax[3]));

        auto&& staminaColorMin = splitString(ini["Style"].get("StaminaColorMin"), ",");
        assert(staminaColorMin.size() == 4);
        BarStyle::staminaMinColor = ImVec4(safe_stof(staminaColorMin[0]), safe_stof(staminaColorMin[1]), safe_stof(staminaColorMin[2]), safe_stof(staminaColorMin[3]));

        auto&& poisonColorMax = splitString(ini["Style"].get("PoisonColorMax"), ",");
        assert(poisonColorMax.size() == 4);
        BarStyle::poisonMaxColor = ImVec4(safe_stof(poisonColorMax[0]), safe_stof(poisonColorMax[1]), safe_stof(poisonColorMax[2]), safe_stof(poisonColorMax[3]));

        auto&& poisonColorMin = splitString(ini["Style"].get("PoisonColorMin"), ",");
        assert(poisonColorMin.size() == 4);
        BarStyle::poisonMinColor = ImVec4(safe_stof(poisonColorMin[0]), safe_stof(poisonColorMin[1]), safe_stof(poisonColorMin[2]), safe_stof(poisonColorMin[3]));

        auto&& rotColorMax = splitString(ini["Style"].get("RotColorMax"), ",");
        assert(rotColorMax.size() == 4);
        BarStyle::rotMaxColor = ImVec4(safe_stof(rotColorMax[0]), safe_stof(rotColorMax[1]), safe_stof(rotColorMax[2]), safe_stof(rotColorMax[3]));

        auto&& rotColorMin = splitString(ini["Style"].get("RotColorMin"), ",");
        assert(rotColorMin.size() == 4);
        BarStyle::rotMinColor = ImVec4(safe_stof(rotColorMin[0]), safe_stof(rotColorMin[1]), safe_stof(rotColorMin[2]), safe_stof(rotColorMin[3]));

        auto&& bleedColorMax = splitString(ini["Style"].get("BleedColorMax"), ",");
        assert(bleedColorMax.size() == 4);
        BarStyle::bleedMaxColor = ImVec4(safe_stof(bleedColorMax[0]), safe_stof(bleedColorMax[1]), safe_stof(bleedColorMax[2]), safe_stof(bleedColorMax[3]));

        auto&& bleedColorMin = splitString(ini["Style"].get("BleedColorMin"), ",");
        assert(bleedColorMin.size() == 4);
        BarStyle::bleedMinColor = ImVec4(safe_stof(bleedColorMin[0]), safe_stof(bleedColorMin[1]), safe_stof(bleedColorMin[2]), safe_stof(bleedColorMin[3]));

        auto&& blightColorMax = splitString(ini["Style"].get("BlightColorMax"), ",");
        assert(blightColorMax.size() == 4);
        BarStyle::blightMaxColor = ImVec4(safe_stof(blightColorMax[0]), safe_stof(blightColorMax[1]), safe_stof(blightColorMax[2]), safe_stof(blightColorMax[3]));

        auto&& blightColorMin = splitString(ini["Style"].get("BlightColorMin"), ",");
        assert(blightColorMin.size() == 4);
        BarStyle::blightMinColor = ImVec4(safe_stof(blightColorMin[0]), safe_stof(blightColorMin[1]), safe_stof(blightColorMin[2]), safe_stof(blightColorMin[3]));

        auto&& frostColorMax = splitString(ini["Style"].get("FrostColorMax"), ",");
        assert(frostColorMax.size() == 4);
        BarStyle::frostMaxColor = ImVec4(safe_stof(frostColorMax[0]), safe_stof(frostColorMax[1]), safe_stof(frostColorMax[2]), safe_stof(frostColorMax[3]));

        auto&& frostColorMin = splitString(ini["Style"].get("FrostColorMin"), ",");
        assert(frostColorMin.size() == 4);
        BarStyle::frostMinColor = ImVec4(safe_stof(frostColorMin[0]), safe_stof(frostColorMin[1]), safe_stof(frostColorMin[2]), safe_stof(frostColorMin[3]));

        auto&& sleepColorMax = splitString(ini["Style"].get("SleepColorMax"), ",");
        assert(sleepColorMax.size() == 4);
        BarStyle::sleepMaxColor = ImVec4(safe_stof(sleepColorMax[0]), safe_stof(sleepColorMax[1]), safe_stof(sleepColorMax[2]), safe_stof(sleepColorMax[3]));

        auto&& sleepColorMin = splitString(ini["Style"].get("SleepColorMin"), ",");
        assert(sleepColorMin.size() == 4);
        BarStyle::sleepMinColor = ImVec4(safe_stof(sleepColorMin[0]), safe_stof(sleepColorMin[1]), safe_stof(sleepColorMin[2]), safe_stof(sleepColorMin[3]));

        auto&& madnessColorMax = splitString(ini["Style"].get("MadnessColorMax"), ",");
        assert(madnessColorMax.size() == 4);
        BarStyle::madnessMaxColor = ImVec4(safe_stof(madnessColorMax[0]), safe_stof(madnessColorMax[1]), safe_stof(madnessColorMax[2]), safe_stof(madnessColorMax[3]));

        //-----------------------------------------------------------------------------------
        //                                        Boss Posture Bar
        //-----------------------------------------------------------------------------------
        BossPostureBarData::drawBars = ini["Boss Posture Bar"].get("DrawBars") == "true";
        BossPostureBarData::useStaminaForNPC = ini["Boss Posture Bar"].get("UseStaminaForNPC") == "true";
        BossPostureBarData::barWidth = safe_stof(ini["Boss Posture Bar"].get("BarWidth"));
        BossPostureBarData::barHeight = safe_stof(ini["Boss Posture Bar"].get("BarHeight"));
        BossPostureBarData::resetStaggerTotalTime = safe_stof(ini["Boss Posture Bar"].get("ResetStaggerTotalTime"));
        BossPostureBarData::firstBossScreenX = safe_stof(ini["Boss Posture Bar"].get("FirstBossScreenX"));
        BossPostureBarData::firstBossScreenY = safe_stof(ini["Boss Posture Bar"].get("FirstBossScreenY"));
        BossPostureBarData::nextBossBarDiffScreenY = safe_stof(ini["Boss Posture Bar"].get("NextBossBarDiffScreenY"));

        BossPostureBarData::alwaysPresent = ini["Boss Posture Bar"].get("AlwaysPresent") == "true";
        BossPostureBarData::drawForOnlyFirst = ini["Boss Posture Bar"].get("DrawForOnlyFirst") == "true";
        BossPostureBarData::drawBar[EERDataType::Stagger] = true;
        BossPostureBarData::drawBar[EERDataType::Stamina] = true;
        BossPostureBarData::drawBar[EERDataType::Poison] = ini["Boss Posture Bar"].get("DrawPoisonBar") == "true";
        BossPostureBarData::drawBar[EERDataType::Rot] = ini["Boss Posture Bar"].get("DrawRotBar") == "true";
        BossPostureBarData::drawBar[EERDataType::Bleed] = ini["Boss Posture Bar"].get("DrawBleedBar") == "true";
        BossPostureBarData::drawBar[EERDataType::Blight] = ini["Boss Posture Bar"].get("DrawBlightBar") == "true";
        BossPostureBarData::drawBar[EERDataType::Frost] = ini["Boss Posture Bar"].get("DrawFrostBar") == "true";
        BossPostureBarData::drawBar[EERDataType::Sleep] = ini["Boss Posture Bar"].get("DrawSleepBar") == "true";
        BossPostureBarData::drawBar[EERDataType::Madness] = ini["Boss Posture Bar"].get("DrawMadnessBar") == "true";
        for (size_t i = static_cast<size_t>(EERDataType::STATUSES) + 1; i < static_cast<size_t>(EERDataType::MAX); ++i)
        {
            BossPostureBarData::drawStatusBars |= BossPostureBarData::drawBar[static_cast<EERDataType>(i)];
        }
        BossPostureBarData::statusBarWidth = safe_stof(ini["Boss Posture Bar"].get("StatusBarWidth"));
        BossPostureBarData::statusBarHeight = safe_stof(ini["Boss Posture Bar"].get("StatusBarHeight"));
        BossPostureBarData::firstStatusBarDiffScreenX = safe_stof(ini["Boss Posture Bar"].get("FirstStatusBarDiffScreenX"));
        BossPostureBarData::firstStatusBarDiffScreenY = safe_stof(ini["Boss Posture Bar"].get("FirstStatusBarDiffScreenY"));
        BossPostureBarData::nextStatusBarDiffScreenX = safe_stof(ini["Boss Posture Bar"].get("NextStatusBarDiffScreenX"));
        BossPostureBarData::nextStatusBarDiffScreenY = safe_stof(ini["Boss Posture Bar"].get("NextStatusBarDiffScreenY"));

        //-----------------------------------------------------------------------------------
        //                                        Entity Posture Bar
        //-----------------------------------------------------------------------------------
        EntityPostureBarData::drawBars = ini["Entity Posture Bar"].get("DrawBars") == "true";
        EntityPostureBarData::useStaminaForNPC = ini["Entity Posture Bar"].get("UseStaminaForNPC") == "true";
        EntityPostureBarData::barWidth = safe_stof(ini["Entity Posture Bar"].get("BarWidth"));
        EntityPostureBarData::barHeight = safe_stof(ini["Entity Posture Bar"].get("BarHeight"));
        EntityPostureBarData::resetStaggerTotalTime = safe_stof(ini["Entity Posture Bar"].get("ResetStaggerTotalTime"));
        EntityPostureBarData::offsetScreenX = safe_stof(ini["Entity Posture Bar"].get("OffsetScreenX"));
        EntityPostureBarData::offsetScreenY = safe_stof(ini["Entity Posture Bar"].get("OffsetScreenY"));
        EntityPostureBarData::leftScreenThreshold = safe_stof(ini["Entity Posture Bar"].get("LeftScreenThreshold"));
        EntityPostureBarData::rightScreenThreshold = safe_stof(ini["Entity Posture Bar"].get("RightScreenThreshold"));
        EntityPostureBarData::topScreenThreshold = safe_stof(ini["Entity Posture Bar"].get("TopScreenThreshold"));
        EntityPostureBarData::bottomScreenThreshold = safe_stof(ini["Entity Posture Bar"].get("BottomScreenThreshold"));
        EntityPostureBarData::usePositionFixing = ini["Entity Posture Bar"].get("UsePositionFixing") == "true";
        EntityPostureBarData::positionFixingMultiplierX = safe_stof(ini["Entity Posture Bar"].get("PositionFixingMultiplierX"));
        EntityPostureBarData::positionFixingMultiplierY = safe_stof(ini["Entity Posture Bar"].get("PositionFixingMultiplierY"));
        EntityPostureBarData::onlyTarget = ini["Entity Posture Bar"].get("OnlyTarget") == "true";

        EntityPostureBarData::alwaysPresent = ini["Entity Posture Bar"].get("AlwaysPresent") == "true";
        EntityPostureBarData::drawBar[EERDataType::Stagger] = true;
        EntityPostureBarData::drawBar[EERDataType::Stamina] = true;
        EntityPostureBarData::drawBar[EERDataType::Poison] = ini["Entity Posture Bar"].get("DrawPoisonBar") == "true";
        EntityPostureBarData::drawBar[EERDataType::Rot] = ini["Entity Posture Bar"].get("DrawRotBar") == "true";
        EntityPostureBarData::drawBar[EERDataType::Bleed] = ini["Entity Posture Bar"].get("DrawBleedBar") == "true";
        EntityPostureBarData::drawBar[EERDataType::Blight] = ini["Entity Posture Bar"].get("DrawBlightBar") == "true";
        EntityPostureBarData::drawBar[EERDataType::Frost] = ini["Entity Posture Bar"].get("DrawFrostBar") == "true";
        EntityPostureBarData::drawBar[EERDataType::Sleep] = ini["Entity Posture Bar"].get("DrawSleepBar") == "true";
        EntityPostureBarData::drawBar[EERDataType::Madness] = ini["Entity Posture Bar"].get("DrawMadnessBar") == "true";
        for (size_t i = static_cast<size_t>(EERDataType::STATUSES) + 1; i < static_cast<size_t>(EERDataType::MAX); ++i)
        {
            EntityPostureBarData::drawStatusBars |= EntityPostureBarData::drawBar[static_cast<EERDataType>(i)];
        }
        EntityPostureBarData::statusBarWidth = safe_stof(ini["Entity Posture Bar"].get("StatusBarWidth"));
        EntityPostureBarData::statusBarHeight = safe_stof(ini["Entity Posture Bar"].get("StatusBarHeight"));
        EntityPostureBarData::firstStatusBarDiffScreenX = safe_stof(ini["Entity Posture Bar"].get("FirstStatusBarDiffScreenX"));
        EntityPostureBarData::firstStatusBarDiffScreenY = safe_stof(ini["Entity Posture Bar"].get("FirstStatusBarDiffScreenY"));
        EntityPostureBarData::nextStatusBarDiffScreenX = safe_stof(ini["Entity Posture Bar"].get("NextStatusBarDiffScreenX"));
        EntityPostureBarData::nextStatusBarDiffScreenY = safe_stof(ini["Entity Posture Bar"].get("NextStatusBarDiffScreenY"));

        //-----------------------------------------------------------------------------------
        //                                        Experimental
        //-----------------------------------------------------------------------------------
        hideBarsOnMenu = ini["Experimental"].get("HideBarsOnMenu") == "true";
        PlayerPostureBarData::drawBar = ini["Experimental"].get("DrawBar") == "true";
        PlayerPostureBarData::barWidth = safe_stof(ini["Experimental"].get("BarWidth"));
        PlayerPostureBarData::barHeight = safe_stof(ini["Experimental"].get("BarHeight"));
        PlayerPostureBarData::resetStaggerTotalTime = safe_stof(ini["Experimental"].get("ResetStaggerTotalTime"));
        PlayerPostureBarData::screenX = safe_stof(ini["Experimental"].get("ScreenX"));
        PlayerPostureBarData::screenY = safe_stof(ini["Experimental"].get("ScreenY"));

        //-----------------------------------------------------------------------------------
        //                                     Status Icons
        //-----------------------------------------------------------------------------------
        if (ini.has("Status Icons"))
        {
            if (ini["Status Icons"].has("DrawStatusIcons"))
                StatusIconConfig::drawStatusIcons = ini["Status Icons"].get("DrawStatusIcons") != "false";

            if (ini["Status Icons"].has("EntityIconPosition"))
            {
                std::string pos = ini["Status Icons"].get("EntityIconPosition");
                for (char& c : pos) { if (c >= 'A' && c <= 'Z') c += ('a' - 'A'); }
                StatusIconConfig::entityIconsTop = (pos != "bottom");
            }
            if (ini["Status Icons"].has("EntityIconSize"))
                StatusIconConfig::entityIconSize = safe_stof(ini["Status Icons"].get("EntityIconSize"));
            if (ini["Status Icons"].has("EntityIconOffsetY"))
                StatusIconConfig::entityIconOffsetY = safe_stof(ini["Status Icons"].get("EntityIconOffsetY"));

            if (ini["Status Icons"].has("BossIconPosition"))
            {
                std::string pos = ini["Status Icons"].get("BossIconPosition");
                for (char& c : pos) { if (c >= 'A' && c <= 'Z') c += ('a' - 'A'); }
                StatusIconConfig::bossIconsTop = (pos != "bottom");
            }
            if (ini["Status Icons"].has("BossIconSize"))
                StatusIconConfig::bossIconSize = safe_stof(ini["Status Icons"].get("BossIconSize"));
            if (ini["Status Icons"].has("BossIconOffsetY"))
                StatusIconConfig::bossIconOffsetY = safe_stof(ini["Status Icons"].get("BossIconOffsetY"));
        }


        //-----------------------------------------------------------------------------------
        //                                        Debug
        //-----------------------------------------------------------------------------------
        Logger::useLogger = ini["Debug"].get("Log") == "true";
        offsetTesting = ini["Debug"].get("OffsetTest") == "true";
    }
    catch(const std::exception& e)
    {
        Logger::useLogger = true;
        Logger::log(e.what(), LogLevel::Error);
        return false;
    }
    catch (...) 
    {
        Logger::useLogger = true;
        Logger::log("Unknown exception during loading of PostureBarModConfig.ini", LogLevel::Error);
        return false;
    }

    Logger::log("Dll path: " + dllPath);
    Logger::log("Loaded ini settings: ");
    Logger::log("\tGeneral:");
    Logger::log("\t\tInGameScreenCoordWidth: " + std::to_string(ScreenParams::inGameCoordSizeX));
    Logger::log("\t\tInGameScreenCoordHeight: " + std::to_string(ScreenParams::inGameCoordSizeY));
    Logger::log("\t\tScreenPositionX: " + std::to_string(ScreenParams::posX));
    Logger::log("\t\tScreenPositionY: " + std::to_string(ScreenParams::posY));
    Logger::log("\t\tGameToScreenScaling: " + std::to_string(ScreenParams::gameToViewportScaling));
    Logger::log("\t\tAutoPositionSetup: " + std::to_string(ScreenParams::autoPositionSetup));
    Logger::log("\t\tAutoGameToScreenScaling: " + std::to_string(ScreenParams::autoGameToViewportScaling));
    Logger::log("\tTextures:");
    Logger::log("\t\tUseTextures: " + std::to_string(TextureData::useTextures));
    Logger::log("\t\tBossBarFile: " + TextureFileData::bossBarFile);
    Logger::log("\t\tBossBorderFile: " + TextureFileData::bossBorderFile);
    Logger::log("\t\tEntityBarFile: " + TextureFileData::entityBarFile);
    Logger::log("\t\tEntityBorderFile: " + TextureFileData::entityBorderFile);
    Logger::log("\t\tBossFillOffset: " + std::to_string(TextureData::bossOffset));
    Logger::log("\t\tEntityFillOffset: " + std::to_string(TextureData::entityOffset));
    Logger::log("\t\tCircleFillOffset: " + std::to_string(TextureData::circleOffset));
    Logger::log("\tStyle:");
    Logger::log("\t\tStatusBarShape: " + std::to_string((int)BarStyle::statusBarShape));
    Logger::log("\t\tFillAlignment: " + std::to_string((int)BarStyle::fillAlignment));
    Logger::log("\t\tFillType: " + std::to_string((int)BarStyle::fillType));
    Logger::log("\t\tFillResizeType: " + std::to_string((int)BarStyle::fillResizeType));
    Logger::log("\t\tStaggerMaxColor: " + std::to_string(BarStyle::staggerMaxColor));
    Logger::log("\t\tStaggerMinColor: " + std::to_string(BarStyle::staggerMinColor));
    Logger::log("\t\tStaminaMaxColor: " + std::to_string(BarStyle::staminaMaxColor));
    Logger::log("\t\tStaminaMinColor: " + std::to_string(BarStyle::staminaMinColor));
    Logger::log("\tBoss Posture Bar:");
    Logger::log("\t\tDrawBars: " + std::to_string(BossPostureBarData::drawBars));
    Logger::log("\t\tUseStaminaForNPC: " + std::to_string(BossPostureBarData::useStaminaForNPC));
    Logger::log("\t\tBarWidth: " + std::to_string(BossPostureBarData::barWidth));
    Logger::log("\t\tBarHeight: " + std::to_string(BossPostureBarData::barHeight));
    Logger::log("\t\tResetStaggerTotalTime: " + std::to_string(BossPostureBarData::resetStaggerTotalTime));
    Logger::log("\t\tFirstBossScreenX: " + std::to_string(BossPostureBarData::firstBossScreenX));
    Logger::log("\t\tFirstBossScreenY: " + std::to_string(BossPostureBarData::firstBossScreenY));
    Logger::log("\t\tNextBossBarDiffScreenY: " + std::to_string(BossPostureBarData::nextBossBarDiffScreenY));
    Logger::log("\t\tAlwaysPresent: " + std::to_string(BossPostureBarData::alwaysPresent));
    Logger::log("\t\tDrawForOnlyFirst: " + std::to_string(BossPostureBarData::drawForOnlyFirst));
    Logger::log("\t\tDrawPoisonBar: " + std::to_string(BossPostureBarData::drawBar[EERDataType::Poison]));
    Logger::log("\t\tDrawRotBar: " + std::to_string(BossPostureBarData::drawBar[EERDataType::Rot]));
    Logger::log("\t\tDrawBleedBar: " + std::to_string(BossPostureBarData::drawBar[EERDataType::Bleed]));
    Logger::log("\t\tDrawBlightBar: " + std::to_string(BossPostureBarData::drawBar[EERDataType::Blight]));
    Logger::log("\t\tDrawFrostBar: " + std::to_string(BossPostureBarData::drawBar[EERDataType::Frost]));
    Logger::log("\t\tDrawSleepBar: " + std::to_string(BossPostureBarData::drawBar[EERDataType::Sleep]));
    Logger::log("\t\tDrawMadnessBar: " + std::to_string(BossPostureBarData::drawBar[EERDataType::Madness]));
    Logger::log("\t\tDrawStatusBars: " + std::to_string(BossPostureBarData::drawStatusBars));
    Logger::log("\t\tStatusBarWidth: " + std::to_string(BossPostureBarData::statusBarWidth));
    Logger::log("\t\tStatusBarHeight: " + std::to_string(BossPostureBarData::statusBarHeight));
    Logger::log("\t\tFirstStatusBarDiffScreenX: " + std::to_string(BossPostureBarData::firstStatusBarDiffScreenX));
    Logger::log("\t\tFirstStatusBarDiffScreenY: " + std::to_string(BossPostureBarData::firstStatusBarDiffScreenY));
    Logger::log("\t\tNextStatusBarDiffScreenX: " + std::to_string(BossPostureBarData::nextStatusBarDiffScreenX));
    Logger::log("\t\tNextStatusBarDiffScreenY: " + std::to_string(BossPostureBarData::nextStatusBarDiffScreenY));
    Logger::log("\tEntity Posture Bar:");
    Logger::log("\t\tDrawBars: " + std::to_string(EntityPostureBarData::drawBars));
    Logger::log("\t\tUseStaminaForNPC: " + std::to_string(EntityPostureBarData::useStaminaForNPC));
    Logger::log("\t\tBarWidth: " + std::to_string(EntityPostureBarData::barWidth));
    Logger::log("\t\tBarHeight: " + std::to_string(EntityPostureBarData::barHeight));
    Logger::log("\t\tResetStaggerTotalTime: " + std::to_string(EntityPostureBarData::resetStaggerTotalTime));
    Logger::log("\t\tOffsetScreenX: " + std::to_string(EntityPostureBarData::offsetScreenX));
    Logger::log("\t\tOffsetScreenY: " + std::to_string(EntityPostureBarData::offsetScreenY));
    Logger::log("\t\tLeftScreenThreshold: " + std::to_string(EntityPostureBarData::leftScreenThreshold));
    Logger::log("\t\tRightScreenThreshold: " + std::to_string(EntityPostureBarData::rightScreenThreshold));
    Logger::log("\t\tTopScreenThreshold: " + std::to_string(EntityPostureBarData::topScreenThreshold));
    Logger::log("\t\tBottomScreenThreshold: " + std::to_string(EntityPostureBarData::bottomScreenThreshold));
    Logger::log("\t\tUsePositionFixing: " + std::to_string(EntityPostureBarData::usePositionFixing));
    Logger::log("\t\tPositionFixingMultiplierX: " + std::to_string(EntityPostureBarData::positionFixingMultiplierX));
    Logger::log("\t\tPositionFixingMultiplierY: " + std::to_string(EntityPostureBarData::positionFixingMultiplierY));
    Logger::log("\t\tAlwaysPresent: " + std::to_string(EntityPostureBarData::alwaysPresent));
    Logger::log("\t\tDrawPoisonBar: " + std::to_string(EntityPostureBarData::drawBar[EERDataType::Poison]));
    Logger::log("\t\tDrawRotBar: " + std::to_string(EntityPostureBarData::drawBar[EERDataType::Rot]));
    Logger::log("\t\tDrawBleedBar: " + std::to_string(EntityPostureBarData::drawBar[EERDataType::Bleed]));
    Logger::log("\t\tDrawBlightBar: " + std::to_string(EntityPostureBarData::drawBar[EERDataType::Blight]));
    Logger::log("\t\tDrawFrostBar: " + std::to_string(EntityPostureBarData::drawBar[EERDataType::Frost]));
    Logger::log("\t\tDrawSleepBar: " + std::to_string(EntityPostureBarData::drawBar[EERDataType::Sleep]));
    Logger::log("\t\tDrawMadnessBar: " + std::to_string(EntityPostureBarData::drawBar[EERDataType::Madness]));
    Logger::log("\t\tDrawStatusBars: " + std::to_string(EntityPostureBarData::drawStatusBars));
    Logger::log("\t\tStatusBarWidth: " + std::to_string(EntityPostureBarData::statusBarWidth));
    Logger::log("\t\tStatusBarHeight: " + std::to_string(EntityPostureBarData::statusBarHeight));
    Logger::log("\t\tFirstStatusBarDiffScreenX: " + std::to_string(EntityPostureBarData::firstStatusBarDiffScreenX));
    Logger::log("\t\tFirstStatusBarDiffScreenY: " + std::to_string(EntityPostureBarData::firstStatusBarDiffScreenY));
    Logger::log("\t\tNextStatusBarDiffScreenX: " + std::to_string(EntityPostureBarData::nextStatusBarDiffScreenX));
    Logger::log("\t\tNextStatusBarDiffScreenY: " + std::to_string(EntityPostureBarData::nextStatusBarDiffScreenY));
    Logger::log("\tExperimental:");
    Logger::log("\t\tHideBarsOnMenu: " + std::to_string(hideBarsOnMenu));
    Logger::log("\t\tDrawBar: " + std::to_string(PlayerPostureBarData::drawBar));
    Logger::log("\t\tBarWidth: " + std::to_string(PlayerPostureBarData::barWidth));
    Logger::log("\t\tBarHeight: " + std::to_string(PlayerPostureBarData::barHeight));
    Logger::log("\t\tResetStaggerTotalTime: " + std::to_string(PlayerPostureBarData::resetStaggerTotalTime));
    Logger::log("\t\tScreenX: " + std::to_string(PlayerPostureBarData::screenX));
    Logger::log("\t\tScreenY: " + std::to_string(PlayerPostureBarData::screenY));
    Logger::log("\tDebug:");
    Logger::log("\t\tLog: " + std::to_string(Logger::useLogger));
    Logger::log("\t\tOffsetTest: " + std::to_string(offsetTesting));

    return true;
}

bool saveTestOffsetToIni()
{
    try
    {
        using namespace mINI;
        auto safe_stof = [](const std::string& str, float default_val = 0.0f) -> float {
            if (str.empty()) return default_val;
            try {
                std::string s = str;
                while (!s.empty() && (s.back() == 'f' || s.back() == 'F' || s.back() == ' ' || s.back() == '\t'))
                    s.pop_back();
                size_t start = s.find_first_not_of(" \t");
                if (start == std::string::npos) return default_val;
                return std::stof(s.substr(start));
            } catch (...) {
                return default_val;
            }
        };

        auto safe_stoi = [](const std::string& str, int default_val = 0) -> int {
            if (str.empty()) return default_val;
            try {
                std::string s = str;
                size_t start = s.find_first_not_of(" \t");
                if (start == std::string::npos) return default_val;
                return std::stoi(s.substr(start));
            } catch (...) {
                return default_val;
            }
        };
        INIFile config(dllPath + "mods\\PostureBarModConfig.ini");
        INIStructure ini;

        if (!config.read(ini))
            throw std::runtime_error("Failed to read PostureModConfig.ini in mods folder");

        switch (debugState)
        {
            using enum EDebugTestState;

            case BossBarOffset:
                ini["Boss Posture Bar"]["FirstBossScreenX"] = std::to_string(BossPostureBarData::firstBossScreenX);
                ini["Boss Posture Bar"]["FirstBossScreenY"] = std::to_string(BossPostureBarData::firstBossScreenY);
                break;
            case EntityBarOffset:
                ini["Entity Posture Bar"]["OffsetScreenX"] = std::to_string(EntityPostureBarData::offsetScreenX);
                ini["Entity Posture Bar"]["OffsetScreenY"] = std::to_string(EntityPostureBarData::offsetScreenY);
                break;
            case GameScreenOffset:
                ini["General"]["ScreenPositionX"] = std::to_string(ScreenParams::posX);
                ini["General"]["ScreenPositionY"] = std::to_string(ScreenParams::posY);
                break;
            case PosFixingMultiplier:
                ini["Entity Posture Bar"]["PositionFixingMultiplierX"] = std::to_string((float)EntityPostureBarData::positionFixingMultiplierX);
                ini["Entity Posture Bar"]["PositionFixingMultiplierY"] = std::to_string((float)EntityPostureBarData::positionFixingMultiplierY);
                break;
        }

        config.write(ini, true);
    }
    catch (const std::exception& e)
    {
        Logger::useLogger = true;
        Logger::log(e.what());
        return false;
    }
    catch (...)
    {
        Logger::useLogger = true;
        Logger::log("Unknown exception during saving of PostureBarModConfig.ini");
        return false;
    }

    return true;
}

bool findDllPath()
{
    try
    {
        char path[MAX_PATH];
        HMODULE moduleHandle = NULL;

        // Get the handle to this module (the current DLL)
        if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&findDllPath),
            &moduleHandle))
            throw std::runtime_error("Failed to find dll module handle");

        // Get the path of the current DLL
        if (!GetModuleFileNameA(moduleHandle, path, sizeof(path)))
            throw std::runtime_error("Failed to get module name");

        dllPath = path;

        size_t lastBackslashPos = dllPath.find_last_of("\\");
        if (lastBackslashPos != std::string::npos)
        {
            // Remove the file name from the path
            dllPath.erase(lastBackslashPos + 1);
        }
    }
    catch (const std::exception& e)
    {
        Logger::useLogger = true;
        Logger::log(e.what());
        return false;
    }
    catch (...)
    {
        Logger::useLogger = true;
        Logger::log("Unknown exception during finding dll module");
        return false;
    }

    return true;
}

void MainThread()
{
    if (!findDllPath())
        return;

    if (!loadIni())
        return;

    // Wait until Elden Ring game window is created and ready
    while (!FindERWindow())
    {
        std::this_thread::sleep_for(500ms);
    }
    // Give Streamline and PureDark DLSS FrameGen 4 seconds to finalize proxy setup
    std::this_thread::sleep_for(4s);

    Logger::log("Starting Main Thread");
    try
    {
        g_D3DRenderer = std::make_unique<D3DRenderer>();
        g_D3DRenderer->loadBarTextures();
        g_postureUI = std::make_unique<PostureBarUI>();
        g_Hooking = std::make_unique<Hooking>();
        g_Hooking->Hook();
    }
    catch (const std::exception& ex)
    {
        Logger::useLogger = true;
        Logger::log("Exception during MainThread initialization: " + std::string(ex.what()), LogLevel::Error);
        return;
    }
    catch (...)
    {
        Logger::useLogger = true;
        Logger::log("Unknown exception during MainThread initialization", LogLevel::Error);
        return;
    }

    Logger::log("Starting Main Loop");
    std::pair<float, float> previousMoveVec{1.f, 1.f};
    int counter = 0;
    while (g_Running)
    {
        if (offsetTesting)
        {
            std::pair<float, float> moveVec{ float((GetAsyncKeyState(VK_RIGHT) & 1) - (GetAsyncKeyState(VK_LEFT) & 1)), float((GetAsyncKeyState(VK_DOWN) & 1) - (GetAsyncKeyState(VK_UP) & 1)) };

            if (counter <= 0)
                offsetSpeed = 0.5f;
            else
                offsetSpeed = std::min(5.f, offsetSpeed + 0.5f);

            if (previousMoveVec == moveVec)
                counter = 20;
            else
                counter = std::max(0, counter - 1);

            switch (debugState)
            {
                using enum EDebugTestState;

                case BossBarOffset:
                    BossPostureBarData::firstBossScreenX += moveVec.first * offsetSpeed;
                    BossPostureBarData::firstBossScreenY += moveVec.second * offsetSpeed;
                    break;
                case EntityBarOffset:
                    EntityPostureBarData::offsetScreenX += moveVec.first * offsetSpeed;
                    EntityPostureBarData::offsetScreenY += moveVec.second * offsetSpeed;
                    break;
                case GameScreenOffset:
                    ScreenParams::posX += moveVec.first * offsetSpeed;
                    ScreenParams::posY += moveVec.second * offsetSpeed;
                    break;
                case PosFixingMultiplier:
                    EntityPostureBarData::positionFixingMultiplierX = std::clamp(EntityPostureBarData::positionFixingMultiplierX + (moveVec.first * std::ceil(offsetSpeed) * 0.1), 0.0, 20.0);
                    EntityPostureBarData::positionFixingMultiplierY = std::clamp(EntityPostureBarData::positionFixingMultiplierY - (moveVec.second * std::ceil(offsetSpeed) * 0.1), 0.0, 20.0);
                    break;
            }

            if (GetAsyncKeyState(VK_INSERT) & 1)
                saveTestOffsetToIni();

            if (GetAsyncKeyState(VK_NEXT) & 1)
                debugState = cycleState(debugState, 1);

            if (GetAsyncKeyState(VK_PRIOR) & 1)
                debugState = cycleState(debugState, -1);

            if (moveVec.first != 0.f || moveVec.second != 0.f)
                previousMoveVec = moveVec;
        }

        // reload ini settings for colors and bar toggles
        if (GetAsyncKeyState(VK_RETURN)) {
            loadIni();
        }

        std::this_thread::sleep_for(3ms);
        std::this_thread::yield();
    }

    std::this_thread::sleep_for(500ms);
    FreeLibraryAndExitThread(g_Module, 0);
}