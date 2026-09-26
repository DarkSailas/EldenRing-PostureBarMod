#include "PostureBarUI.hpp"
#include "Logger.hpp"
#include "Hooking.hpp"
#include "D3DRenderer.hpp"
#include "../PostureBarMod.hpp"
#include <algorithm>

#ifndef __UINT32_MAX__
#define __UINT32_MAX__ (4294967295)
#endif

#ifndef __UINT64_MAX__
#define __UINT64_MAX__ (18446744073709551615)
#endif

namespace ER 
{
    static inline bool isSafeReadable(uintptr_t ptr, size_t size = 8)
    {
        return (ptr >= 0x10000 && ptr <= 0x7FFFFFFFFFFF && (ptr & 0x1) == 0);
    }

    static inline bool isSafeMemory(uintptr_t ptr)
    {
        return isSafeReadable(ptr);
    }

    ImVec2 positionFixOffset(const EntityPostureBarData& postureBar, const std::chrono::steady_clock::time_point& directXtimePoint)
    {
        float gameUIDelta = std::chrono::duration_cast<std::chrono::duration<float>>(postureBar.gameUiUpdateTimePoint - postureBar.gamePreviousUiUpdateTimePoint).count();
        float gameToDirectXDelta = std::chrono::duration_cast<std::chrono::duration<float>>(directXtimePoint - postureBar.gameUiUpdateTimePoint).count();
        float velX = 0.0f;
        float velY = 0.0f;
        if (gameUIDelta > 0)
        {
            velX = ((postureBar.screenX - postureBar.previousScreenX) * (float)EntityPostureBarData::positionFixingMultiplierX) / gameUIDelta;
            velY = ((postureBar.screenY - postureBar.previousScreenY) * (float)EntityPostureBarData::positionFixingMultiplierY) / gameUIDelta;
        }

        return ImVec2(velX * gameToDirectXDelta, velY * gameToDirectXDelta);
    }

    void PostureBarUI::Draw()
    {
        std::lock_guard<std::mutex> lock(dataMutex);

        auto viewportSize = ImGui::GetMainViewport()->Size;
        if (viewportSize.x > 100.0f && viewportSize.y > 100.0f)
        {
            float viewportScaleX = viewportSize.x / ScreenParams::inGameCoordSizeX;
            float viewportScaleY = viewportSize.y / ScreenParams::inGameCoordSizeY;

            if (ScreenParams::autoGameToViewportScaling)
            {
                ScreenParams::gameToViewportScaling = std::min(viewportScaleX, viewportScaleY);
            }

            if (ScreenParams::autoPositionSetup)
            {
                ScreenParams::posX = (viewportSize.x - std::ceilf(ScreenParams::inGameCoordSizeX * ScreenParams::gameToViewportScaling)) * 0.5f;
                ScreenParams::posY = (viewportSize.y - std::ceilf(ScreenParams::inGameCoordSizeY * ScreenParams::gameToViewportScaling)) * 0.5f;
            }
        }

        if (offsetTesting)
        {
            auto debugInfo = [](EDebugTestState state)
            {
                switch (state)
                {
                    using enum EDebugTestState;

                case BossBarOffset:
                    return std::tuple{ "Boss bar screen offset", BossPostureBarData::firstBossScreenX, BossPostureBarData::firstBossScreenY };
                case EntityBarOffset:
                    return std::tuple{ "Entity bar screen offset", EntityPostureBarData::offsetScreenX, EntityPostureBarData::offsetScreenY };
                case GameScreenOffset:
                    return std::tuple{ "Screen offset (white border should match game viewport)", ScreenParams::posX, ScreenParams::posY };
                case PosFixingMultiplier:
                    return std::tuple{ "Position fixing (set lower if bar are getting ahead, set bigger if they are dragging)", (float)EntityPostureBarData::positionFixingMultiplierX, (float)EntityPostureBarData::positionFixingMultiplierY };
                default:
                    return std::tuple{ "", -1.f, -1.f };
                }
            };

            auto viewportSize = ImGui::GetMainViewport()->Size;
            float adjustedViewportX = std::ceilf(ScreenParams::inGameCoordSizeX * ScreenParams::gameToViewportScaling);
            float adjustedViewportY = std::ceilf(ScreenParams::inGameCoordSizeY * ScreenParams::gameToViewportScaling);

            auto [text, x, y] = debugInfo(debugState);
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(20, 15), ImColor(255, 255, 255, 255), "Offset Testing, turn off in .ini file if not needed");
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(20, 35), ImColor(255, 255, 255, 255), "Press insert key to save current offset to .ini file. Press page up/down key to swap tested offset (boss bar, entity bar, screen offset, position fixing properties)");
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(20, 55), ImColor(255, 255, 255, 255), "Press keyboard arrows to change values: X (left/right) Y(down/up)");
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(20, 75), ImColor(255, 255, 255, 255), std::string("Window viewport: " + std::to_string((int)viewportSize.x) + "x" + std::to_string((int)viewportSize.y)).c_str());
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(20, 95), ImColor(255, 255, 255, 255), std::string("Adjusted Window viewport (white border): " + std::to_string((int)adjustedViewportX) + "x" + std::to_string((int)adjustedViewportY)).c_str());
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(20, 115), ImColor(255, 255, 255, 255), std::string(std::string("Game to Shown viewport Scale: ") + std::to_string(ScreenParams::gameToViewportScaling)).c_str());
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(20, 135), ImColor(255, 255, 255, 255), std::string(std::string("Currently testing: ") + text).c_str());
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(20, 155), ImColor(255, 255, 255, 255), std::string("X: " + std::to_string(x)).c_str());
            ImGui::GetBackgroundDrawList()->AddText(ImVec2(130, 155), ImColor(255, 255, 255, 255), std::string("Y: " + std::to_string(y)).c_str());
            ImGui::GetBackgroundDrawList()->AddRect(ImVec2(ScreenParams::posX, ScreenParams::posY), ImVec2(ScreenParams::posX + adjustedViewportX, ScreenParams::posY + adjustedViewportY), ImColor(255, 255, 255,255), 0, 0, 1.0f);
        }

        if (isMenuOpen())
        {
            Logger::log("Menu is open - not rendering bars", LogLevel::Debug);
            return;
        }
        
        if (PlayerPostureBarData::drawBar)
        {
            if (auto _playerPostureBar = playerPostureBar; _playerPostureBar)
            {
                float height = PlayerPostureBarData::barHeight * ScreenParams::gameToViewportScaling;
                float width = PlayerPostureBarData::barWidth * ScreenParams::gameToViewportScaling;
                ImVec2 barSize = ImVec2(width, height);
                ImVec2 positionOnViewport = ImVec2(PlayerPostureBarData::screenX, PlayerPostureBarData::screenY) * ScreenParams::gameToViewportScaling;

                // apply screen position offset
                positionOnViewport.x += ScreenParams::posX;
                positionOnViewport.y += ScreenParams::posY;

                if (_playerPostureBar->isResetStagger)
                {
                    float timeRatio = 1.0f - _playerPostureBar->resetStaggerTimer / PlayerPostureBarData::resetStaggerTotalTime;

                    drawBar(EPostureBarType::Entity, EERDataType::Stagger, positionOnViewport, barSize, timeRatio);
                }
                else
                {
                    float fillRatio = _playerPostureBar->stagger / _playerPostureBar->maxStagger;

                    drawBar(EPostureBarType::Entity, EERDataType::Stagger, positionOnViewport, barSize, fillRatio);
                }
            }
        }

        if (BossPostureBarData::drawBars || StatusIconConfig::drawStatusIcons)
        {
            bool drawnOneStatusBar = false;

            for (IndexType i = 0; i < BOSS_CHR_ARRAY_LEN; i++)
            {
                if (auto bossPostureBar = bossPostureBars[i]; bossPostureBar && bossPostureBar->isVisible)
                {
                    float height = BossPostureBarData::barHeight * ScreenParams::gameToViewportScaling;
                    float width = BossPostureBarData::barWidth * ScreenParams::gameToViewportScaling;
                    ImVec2 barSize = ImVec2(width, height);
                    ImVec2 viewportPosition = ImVec2(BossPostureBarData::firstBossScreenX, BossPostureBarData::firstBossScreenY) * ScreenParams::gameToViewportScaling;

                    // apply screen position offset
                    viewportPosition.x += ScreenParams::posX;
                    viewportPosition.y += ScreenParams::posY;

                    // apply offset if bar is for second and third boss
                    viewportPosition.y -= ((float)i * BossPostureBarData::nextBossBarDiffScreenY) * ScreenParams::gameToViewportScaling;

                    if (bossPostureBar->hasValidBar && BossPostureBarData::drawBars)
                    {
                        if (bossPostureBar->isResetStagger)
                        {
                            float timeRatio = 1.0f - bossPostureBar->resetStaggerTimer / BossPostureBarData::resetStaggerTotalTime;

                            drawBar(EPostureBarType::Boss, EERDataType::Stagger, viewportPosition, barSize, timeRatio);
                        }
                        else
                        {
                            EERDataType erDataType = bossPostureBar->isStamina ? EERDataType::Stamina : EERDataType::Stagger;
                            float fillRatio = bossPostureBar->barDatas[erDataType].GetRatio();

                            drawBar(EPostureBarType::Boss, erDataType, viewportPosition, barSize, fillRatio);
                        }
                    }

                    // Draw active status icons for Boss
                    if (StatusIconConfig::drawStatusIcons)
                    {
                        int activeBossStatusCount = 0;
                        for (int s = 0; s < 7; s++)
                        {
                            if (bossPostureBar->statusIsActive[s])
                                activeBossStatusCount++;
                        }

                        if (activeBossStatusCount > 0)
                        {
                            float iconSize = StatusIconConfig::bossIconSize * ScreenParams::gameToViewportScaling;
                            float iconGap = 6.0f * ScreenParams::gameToViewportScaling;
                            float totalWidth = (activeBossStatusCount * iconSize) + ((activeBossStatusCount - 1) * iconGap);
                            float curX = viewportPosition.x - (barSize.x * 0.5f);
                            float iconY = 0.0f;
                            if (StatusIconConfig::bossIconsTop)
                            {
                                // Top: above Boss HP bar (Boss HP bar is above posture bar)
                                iconY = viewportPosition.y - (30.0f + iconSize + 6.0f) * ScreenParams::gameToViewportScaling + (StatusIconConfig::bossIconOffsetY * ScreenParams::gameToViewportScaling);
                            }
                            else
                            {
                                // Bottom: below Boss Posture bar
                                iconY = viewportPosition.y + (barSize.y + 6.0f) + (StatusIconConfig::bossIconOffsetY * ScreenParams::gameToViewportScaling);
                            }

                            for (int s = 0; s < 7; s++)
                            {
                                if (bossPostureBar->statusIsActive[s])
                                {
                                    ImVec2 pMin(curX, iconY);
                                    ImVec2 pMax(curX + iconSize, iconY + iconSize);

                                    ImGui::GetBackgroundDrawList()->AddRectFilled(pMin, pMax, IM_COL32(12, 12, 12, 220), 4.0f);
                                    if (statusIconTextures[s])
                                    {
                                        ImGui::GetBackgroundDrawList()->AddImage(statusIconTextures[s], pMin, pMax);
                                    }
                                    else
                                    {
                                        const char* letters[7] = { "P", "R", "B", "D", "F", "S", "M" };
                                        ImColor colors[7] = { 
                                            ImColor(80, 200, 80, 240), 
                                            ImColor(200, 80, 40, 240), 
                                            ImColor(220, 20, 20, 240), 
                                            ImColor(180, 180, 20, 240), 
                                            ImColor(40, 180, 240, 240), 
                                            ImColor(180, 40, 200, 240), 
                                            ImColor(240, 200, 20, 240) 
                                        };
                                        ImGui::GetBackgroundDrawList()->AddRectFilled(pMin, pMax, colors[s], 4.0f);
                                        ImGui::GetBackgroundDrawList()->AddText(ImVec2(pMin.x + iconSize * 0.3f, pMin.y + iconSize * 0.15f), IM_COL32(255, 255, 255, 255), letters[s]);
                                    }
                                    ImGui::GetBackgroundDrawList()->AddRect(pMin, pMax, IM_COL32(245, 245, 245, 220), 4.0f, 0, 1.5f);

                                    curX += iconSize + iconGap;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (EntityPostureBarData::drawBars || StatusIconConfig::drawStatusIcons)
        {
            for (IndexType i = 0; i < ENTITY_CHR_ARRAY_LEN; i++)
            {
                if (auto entityPostureBar = entityPostureBars[i]; entityPostureBar && entityPostureBar->isVisible)
                {
                    auto&& timePoint = std::chrono::steady_clock::now();

                    float distanceModifier = entityPostureBar->distanceModifier;

                    ImVec2 gamePosition(entityPostureBar->screenX, entityPostureBar->screenY);

                    // apply fix offset from predicting previous movement
                    if (EntityPostureBarData::usePositionFixing)
                        gamePosition -= positionFixOffset(*entityPostureBar, timePoint);

                    // apply threshold to in game position
                    gamePosition.x = std::clamp(gamePosition.x, EntityPostureBarData::leftScreenThreshold, EntityPostureBarData::rightScreenThreshold);
                    gamePosition.y = std::clamp(gamePosition.y, EntityPostureBarData::topScreenThreshold, EntityPostureBarData::bottomScreenThreshold);

                    // transform in game position into viewport position
                    float height = EntityPostureBarData::barHeight * ScreenParams::gameToViewportScaling;
                    float width = EntityPostureBarData::barWidth * distanceModifier * ScreenParams::gameToViewportScaling;
                    ImVec2 barSize = ImVec2(width, height);
                    ImVec2 viewportPosition = gamePosition * ScreenParams::gameToViewportScaling;

                    // apply screen position offset
                    viewportPosition.x += ScreenParams::posX;
                    viewportPosition.y += ScreenParams::posY;

                    // apply user specified bar offset
                    viewportPosition.x += EntityPostureBarData::offsetScreenX * ScreenParams::gameToViewportScaling;
                    viewportPosition.y += EntityPostureBarData::offsetScreenY * ScreenParams::gameToViewportScaling;

                    if (entityPostureBar->hasValidBar && EntityPostureBarData::drawBars)
                    {
                        if (entityPostureBar->isResetStagger)
                        {
                            float timeRatio = 1.0f - entityPostureBar->resetStaggerTimer / EntityPostureBarData::resetStaggerTotalTime;

                            drawBar(EPostureBarType::Entity, EERDataType::Stagger, viewportPosition, barSize, timeRatio);
                        }
                        else
                        {
                            EERDataType erDataType = entityPostureBar->isStamina ? EERDataType::Stamina : EERDataType::Stagger;
                            float fillRatio = entityPostureBar->barDatas[erDataType].GetRatio();

                            drawBar(EPostureBarType::Entity, erDataType, viewportPosition, barSize, fillRatio);
                        }
                    }

                    // Draw active status icons for Entity
                    if (StatusIconConfig::drawStatusIcons)
                    {
                        int activeEntityStatusCount = 0;
                        for (int s = 0; s < 7; s++)
                        {
                            if (entityPostureBar->statusIsActive[s])
                                activeEntityStatusCount++;
                        }

                        if (activeEntityStatusCount > 0)
                        {
                            float rawSize = StatusIconConfig::entityIconSize * distanceModifier;
                            if (rawSize < 16.0f) rawSize = 16.0f;
                            if (rawSize > 34.0f) rawSize = 34.0f;
                            float iconSize = rawSize * ScreenParams::gameToViewportScaling;
                            float iconGap = 4.0f * ScreenParams::gameToViewportScaling;
                            float totalWidth = (activeEntityStatusCount * iconSize) + ((activeEntityStatusCount - 1) * iconGap);
                            float curX = viewportPosition.x - (barSize.x * 0.5f);
                            float iconY = 0.0f;
                            if (StatusIconConfig::entityIconsTop)
                            {
                                // Top: above enemy HP bar (enemy HP bar is above posture bar)
                                float hpBarHeight = 12.0f * distanceModifier * ScreenParams::gameToViewportScaling;
                                iconY = viewportPosition.y - (hpBarHeight + iconSize + 6.0f) + (StatusIconConfig::entityIconOffsetY * ScreenParams::gameToViewportScaling);
                            }
                            else
                            {
                                // Bottom: below enemy posture bar
                                iconY = viewportPosition.y + (barSize.y + 6.0f) + (StatusIconConfig::entityIconOffsetY * ScreenParams::gameToViewportScaling);
                            }

                            for (int s = 0; s < 7; s++)
                            {
                                if (entityPostureBar->statusIsActive[s])
                                {
                                    ImVec2 pMin(curX, iconY);
                                    ImVec2 pMax(curX + iconSize, iconY + iconSize);

                                    ImGui::GetBackgroundDrawList()->AddRectFilled(pMin, pMax, IM_COL32(12, 12, 12, 220), 3.0f);
                                    if (statusIconTextures[s])
                                    {
                                        ImGui::GetBackgroundDrawList()->AddImage(statusIconTextures[s], pMin, pMax);
                                    }
                                    else
                                    {
                                        const char* letters[7] = { "P", "R", "B", "D", "F", "S", "M" };
                                        ImColor colors[7] = { 
                                            ImColor(80, 200, 80, 240), 
                                            ImColor(200, 80, 40, 240), 
                                            ImColor(220, 20, 20, 240), 
                                            ImColor(180, 180, 20, 240), 
                                            ImColor(40, 180, 240, 240), 
                                            ImColor(180, 40, 200, 240), 
                                            ImColor(240, 200, 20, 240) 
                                        };
                                        ImGui::GetBackgroundDrawList()->AddRectFilled(pMin, pMax, colors[s], 3.0f);
                                        ImGui::GetBackgroundDrawList()->AddText(ImVec2(pMin.x + iconSize * 0.3f, pMin.y + iconSize * 0.15f), IM_COL32(255, 255, 255, 255), letters[s]);
                                    }
                                    ImGui::GetBackgroundDrawList()->AddRect(pMin, pMax, IM_COL32(245, 245, 245, 220), 3.0f, 0, 1.2f);

                                    curX += iconSize + iconGap;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ImColor PostureBarUI::getBarColor(EERDataType erDataType, float fillRatio)
    {
        const auto& [colorFrom, colorTo] = getMinMaxColor(erDataType);

        // so full stagger is "from", and empty is "to"
        fillRatio = 1.0f - fillRatio;
        int r = (int)std::lerp(colorFrom.x, colorTo.x, fillRatio);
        int g = (int)std::lerp(colorFrom.y, colorTo.y, fillRatio);
        int b = (int)std::lerp(colorFrom.z, colorTo.z, fillRatio);
        int a = (int)std::lerp(colorFrom.w, colorTo.w, fillRatio);

        return ImColor(r, g, b, a);
    }

    std::pair<ImVec4, ImVec4> PostureBarUI::getMinMaxColor(EERDataType erDataType)
    {
        switch (erDataType)
        {
            case EERDataType::Stagger:
                return { ER::BarStyle::staggerMinColor, ER::BarStyle::staggerMaxColor };
            case EERDataType::Stamina:
                return { ER::BarStyle::staminaMinColor, ER::BarStyle::staminaMaxColor };
            case EERDataType::Poison:
                return { ER::BarStyle::poisonMinColor, ER::BarStyle::poisonMaxColor };
            case EERDataType::Rot:
                return { ER::BarStyle::rotMinColor, ER::BarStyle::rotMaxColor };
            case EERDataType::Bleed:
                return { ER::BarStyle::bleedMinColor, ER::BarStyle::bleedMaxColor };
            case EERDataType::Blight:
                return { ER::BarStyle::blightMinColor, ER::BarStyle::blightMaxColor };
            case EERDataType::Frost:
                return { ER::BarStyle::frostMinColor, ER::BarStyle::frostMaxColor };
            case EERDataType::Sleep:
                return { ER::BarStyle::sleepMinColor, ER::BarStyle::sleepMaxColor };
            case EERDataType::Madness:
                return { ER::BarStyle::madnessMinColor, ER::BarStyle::madnessMaxColor };
            default:
                throw std::invalid_argument("PostureBarUI::getMinMaxColor - Not supported ER Data Type");
        }
    }

    void PostureBarUI::drawBar(const EPostureBarType postureBarType, const EERDataType erDataType, const ImVec2& position, const ImVec2& size, float fillRatio)
    {
        const ImColor& barColor = getBarColor(erDataType, fillRatio);

        if (BarStyle::statusBarShape == EBarShapeType::Circle && static_cast<size_t>(erDataType) > static_cast<size_t>(EERDataType::STATUSES))
        {
            drawCircleBar(barColor, position, size, fillRatio);
        }
        else if (textureBarInit)
        {
            const TextureBar& textureBar = postureBarType == EPostureBarType::Entity ? entityBarTexture : bossBarTexture;
            const FillTextureOffset& textureOffset = postureBarType == EPostureBarType::Entity ? TextureData::entityOffset : TextureData::bossOffset;
            drawBar(textureBar, barColor, position, size, textureOffset, fillRatio);
        }
        else
        {
            drawBar(barColor, position, size, fillRatio);
        }
    }

    void PostureBarUI::drawBar(const TextureBar& textureBar, const ImColor& color, const ImVec2& position, const ImVec2& size, const std::pair<ImVec2 /* top-left */, ImVec2 /* bot-right */>& fillOffset, float fillRatio)
    {
        auto&& [topLeftFillOffset, botRightFillOffset] = fillOffset;
        auto&& [barTextureBorder, barTextureFill] = textureBar;
        ImVec2 fillScale(size.x / barTextureFill.width, size.y / barTextureFill.height);
        ImVec2 borderScale(size.x / barTextureBorder.width, size.y / barTextureBorder.height);

        ImVec2 fillTopLeftScaled = (ImVec2(-barTextureFill.width * 0.5f, 0.0f) + topLeftFillOffset) * fillScale;
        ImVec2 fillBotRightScaled = (ImVec2(barTextureFill.width * 0.5f, barTextureFill.height) + botRightFillOffset) * fillScale;

        auto getFillPositions = [](float width, float ratio, EFillAlignment alignment, EFillType type) -> std::pair<ImVec2, ImVec2>
        {
            ratio = (type == EFillType::EmptyToFull) ? ratio : 1.0f - ratio;

            switch (alignment)
            {
                using enum EFillAlignment;
                case Left:
                    return { ImVec2(0.0f, 0.0f), ImVec2(-width * ratio, 0.0f) };
                case Center:
                    return { ImVec2(width * ratio * 0.5f, 0.0f), ImVec2(-width * ratio * 0.5f, 0.0f) };
                case Right:
                    return { ImVec2(width * ratio, 1.0f), ImVec2(0.0f, 0.0f) };
                default:
                    throw("EFillAlignment out of bounds index");
            }
        };

        auto&& [leftFill, rightFill] = getFillPositions(std::abs(fillTopLeftScaled.x - fillBotRightScaled.x), fillRatio, BarStyle::fillAlignment, BarStyle::fillType);

        // Add Fill texture with clip to stagger ratio
        if (BarStyle::fillResizeType == EFillResizeType::Clip)
        {
            ImGui::GetBackgroundDrawList()->PushClipRect(position + fillTopLeftScaled + leftFill, position + fillBotRightScaled + rightFill);
            ImGui::GetBackgroundDrawList()->AddImage(barTextureFill.texture, position + fillTopLeftScaled, position + fillBotRightScaled, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), color);
            ImGui::GetBackgroundDrawList()->PopClipRect();
        }
        else if (BarStyle::fillResizeType == EFillResizeType::Scale)
        {
            ImGui::GetBackgroundDrawList()->AddImage(barTextureFill.texture, position + fillTopLeftScaled + leftFill, position + fillBotRightScaled + rightFill, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), color);
        }

        // Add Border texture
        ImGui::GetBackgroundDrawList()->AddImage(barTextureBorder.texture, position - ImVec2(barTextureBorder.width * 0.5f, 0.0f) * borderScale, position + ImVec2(barTextureBorder.width * 0.5f, barTextureBorder.height) * borderScale);
    }

    void PostureBarUI::drawBar(const ImColor& color, const ImVec2& position, const ImVec2& size, float fillRatio)
    {
        ImGui::GetBackgroundDrawList()->AddRectFilled(position, position + size * ImVec2(fillRatio, 1.0f), color);
    }

    void PostureBarUI::drawCircleBar(const ImColor& color, const ImVec2& position, const ImVec2& size, float fillRatio)
    {
        ImVec2 circleFillSize = ImVec2(circleTexture.width, circleTexture.height) + TextureData::circleOffset;
        ImVec2 circleScale(size.x / circleTexture.width, size.y / circleTexture.height);

        drawCircle(color, position + ImVec2(0, circleTexture.height * 0.5f) * circleScale, circleFillSize * 0.5f * circleScale, fillRatio);
        if (textureBarInit)
        {
            ImGui::GetBackgroundDrawList()->AddImage(circleTexture.texture, position - ImVec2(circleTexture.width * 0.5f, 0.0f) * circleScale, position + ImVec2(circleTexture.width * 0.5f, circleTexture.height) * circleScale, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        }
    }

    void PostureBarUI::drawCircle(const ImColor& color, const ImVec2& position, const ImVec2& size, float fillRatio)
    {
        int segments = 64;
        float start_angle = -IM_PI / 2.0f;
        float end_angle = start_angle + fillRatio * 2.0f * IM_PI;

        ImVector<ImVec2> circlePoints;
        circlePoints.push_back(position);

        for (int i = 0; i <= segments; ++i)
        {
            float angle = start_angle + (end_angle - start_angle) * i / segments;
            ImVec2 point = ImVec2(position.x + cosf(angle) * size.x, position.y + sinf(angle) * size.y);
            circlePoints.push_back(point);
        }

        ImGui::GetBackgroundDrawList()->AddCircle(position, fillRatio, color, 64, 2.0f);
        ImGui::GetBackgroundDrawList()->AddConvexPolyFilled(circlePoints.Data, circlePoints.Size, color);
    }

    bool PostureBarUI::isMenuOpen()
    {
        if (!hideBarsOnMenu)
            return false;

        bool isLoad = g_Hooking->isLoading ? RPM<bool>(g_Hooking->isLoading) : false;
        bool menuOpen = false;
        if (g_Hooking->menuState1 && RPM<uint32_t>(g_Hooking->menuState1) != __UINT32_MAX__) menuOpen = true;
        if (g_Hooking->menuState2 && RPM<uint32_t>(g_Hooking->menuState2) != __UINT32_MAX__) menuOpen = true;
        if (g_Hooking->menuState3 && RPM<uint32_t>(g_Hooking->menuState3) != __UINT32_MAX__) menuOpen = true;

        return isLoad || menuOpen;
    }

    static GameData::ResistanceModule* getValidResistanceModule(GameData::ChrModuleBag* cmb)
    {
        if (!cmb || !isSafeMemory((uintptr_t)cmb)) return nullptr;

        if (isSafeMemory((uintptr_t)cmb->resistanceModule))
        {
            auto* rm = cmb->resistanceModule;
            // Any valid resistance module has positive max for at least one status
            if ((rm->poisonMax > 0 && rm->poisonMax < 50000) ||
                (rm->rotMax > 0 && rm->rotMax < 50000) ||
                (rm->bleedMax > 0 && rm->bleedMax < 50000) ||
                (rm->frostMax > 0 && rm->frostMax < 50000) ||
                (rm->sleepMax > 0 && rm->sleepMax < 50000) ||
                (rm->madnessMax > 0 && rm->madnessMax < 50000))
            {
                return rm;
            }
        }

        return nullptr;
    }

    // Full list of status effect IDs extracted from regulation.bin (Vanilla ER 1.14 + Convergence mod)
    // 0: Poison (stateInfo 2)
    static const int status_0_ids[] = { 834, 882, 883, 1622, 1785, 1939, 3120, 3121, 3176, 3178, 3180, 3182, 3307, 3370, 3750, 4002, 4004, 4525, 6500, 6501, 6502, 6503, 6504, 6505, 6510, 6511, 6512, 6513, 6514, 6515, 8571, 11560, 11561, 11605, 20000, 20001, 20002, 20003, 20004, 20005, 20006, 20007, 20008, 20009, 20010, 20011, 20012, 20013, 20014, 20015, 20016, 20017, 20018, 20019, 20110, 20120, 20130, 20140, 20141, 20142, 20150, 20151, 20152, 20160, 20161, 20162, 20163, 20170, 20175, 20180, 20181, 20190, 20500, 20501, 20502, 20503, 20504, 20505, 20506, 20507, 20508, 20509, 20510, 20511, 20512, 20513, 20514, 20515, 20516, 20517, 20518, 20519, 20600, 106000, 106001, 106002, 106003, 106004, 106005, 106006, 106007, 106008, 106009, 106010, 106011, 106012, 106013, 106014, 106015, 106016, 106017, 106018, 106019, 106020, 106021, 106022, 106023, 106024, 106025, 106050, 106051, 106052, 106053, 106054, 106055, 106056, 106057, 106058, 106059, 106060, 106061, 106062, 106063, 106064, 106065, 106066, 106067, 106068, 106069, 106070, 106071, 106072, 106073, 106074, 106075, 106100, 106101, 106102, 106103, 106104, 106105, 106106, 106107, 106108, 106109, 106110, 106111, 106112, 106113, 106114, 106115, 106116, 106117, 106118, 106119, 106120, 106121, 106122, 106123, 106124, 106125, 106150, 106151, 106152, 106153, 106154, 106155, 106156, 106157, 106158, 106159, 106160, 106161, 106162, 106163, 106164, 106165, 106166, 106167, 106168, 106169, 106170, 106171, 106172, 106173, 106174, 106175, 106200, 106201, 106202, 106203, 106204, 106205, 106206, 106207, 106208, 106209, 106210, 106211, 106212, 106213, 106214, 106215, 106216, 106217, 106218, 106219, 106220, 106221, 106222, 106223, 106224, 106225, 106250, 106251, 106252, 106253, 106254, 106255, 106256, 106257, 106258, 106259, 106260, 106261, 106262, 106263, 106264, 106265, 106266, 106267, 106268, 106269, 106270, 106271, 106272, 106273, 106274, 106275, 106300, 106301, 106302, 106303, 106304, 106305, 106306, 106307, 106308, 106309, 106310, 106311, 106312, 106313, 106314, 106315, 106316, 106317, 106318, 106319, 106320, 106321, 106322, 106323, 106324, 106325, 390612, 390617, 500370, 500430, 500431, 500440, 501236, 501720, 501840, 501841, 503580, 1721100, 1722000, 1723001, 1728000, 1728300, 20000820, 20001050, 20004006, 20020100, 20381261, 20500330, 20500331, 20500370 };

    // 1: Rot (stateInfo 5)
    static const int status_1_ids[] = { 3308, 3311, 3313, 3315, 3317, 4051, 4053, 4055, 4555, 6600, 6601, 6602, 6603, 6604, 6605, 11606, 11612, 11613, 13559, 13560, 18430, 18431, 18432, 18433, 21000, 21001, 21002, 21003, 21004, 21005, 21006, 21007, 21008, 21009, 21010, 21011, 21012, 21013, 21014, 21015, 21016, 21017, 21018, 21019, 21100, 21101, 21102, 107000, 107001, 107002, 107003, 107004, 107005, 107006, 107007, 107008, 107009, 107010, 107011, 107012, 107013, 107014, 107015, 107016, 107017, 107018, 107019, 107020, 107021, 107022, 107023, 107024, 107025, 107050, 107051, 107052, 107053, 107054, 107055, 107056, 107057, 107058, 107059, 107060, 107061, 107062, 107063, 107064, 107065, 107066, 107067, 107068, 107069, 107070, 107071, 107072, 107073, 107074, 107075, 107100, 107101, 107102, 107103, 107104, 107105, 107106, 107107, 107108, 107109, 107110, 107111, 107112, 107113, 107114, 107115, 107116, 107117, 107118, 107119, 107120, 107121, 107122, 107123, 107124, 107125, 107150, 107151, 107152, 107153, 107154, 107155, 107156, 107157, 107158, 107159, 107160, 107161, 107162, 107163, 107164, 107165, 107166, 107167, 107168, 107169, 107170, 107171, 107172, 107173, 107174, 107175, 107200, 107201, 107202, 107203, 107204, 107205, 107206, 107207, 107208, 107209, 107210, 107211, 107212, 107213, 107214, 107215, 107216, 107217, 107218, 107219, 107220, 107221, 107222, 107223, 107224, 107225, 107250, 107251, 107252, 107253, 107254, 107255, 107256, 107257, 107258, 107259, 107260, 107261, 107262, 107263, 107264, 107265, 107266, 107267, 107268, 107269, 107270, 107271, 107272, 107273, 107274, 107275, 500670, 1703000, 1703100, 1722200, 1724000, 1724001, 1727000, 1728001, 1728301, 20000862, 20004057, 20010462, 20381262, 20500670, 21720000, 21720001 };

    // 2: Bleed (stateInfo 6)
    static const int status_2_ids[] = { 870, 1615, 1616, 1625, 1753, 1756, 1759, 1761, 1764, 3051, 3115, 3125, 3126, 3191, 3193, 3195, 3197, 3612, 3751, 6400, 6401, 6402, 6403, 6404, 6405, 6406, 6410, 6411, 6412, 6413, 6414, 6415, 8570, 10660, 10661, 10662, 10691, 13944, 22000, 22001, 22002, 22003, 22004, 22005, 22006, 22007, 22008, 22009, 22010, 22011, 22012, 22013, 22014, 22015, 22016, 22017, 22018, 22019, 22101, 22110, 22111, 22120, 22125, 22130, 22131, 22132, 105000, 105001, 105002, 105003, 105004, 105005, 105006, 105007, 105008, 105009, 105010, 105011, 105012, 105013, 105014, 105015, 105016, 105017, 105018, 105019, 105020, 105021, 105022, 105023, 105024, 105025, 105050, 105051, 105052, 105053, 105054, 105055, 105056, 105057, 105058, 105059, 105060, 105061, 105062, 105063, 105064, 105065, 105066, 105067, 105068, 105069, 105070, 105071, 105072, 105073, 105074, 105075, 105100, 105101, 105102, 105103, 105104, 105105, 105106, 105107, 105108, 105109, 105110, 105111, 105112, 105113, 105114, 105115, 105116, 105117, 105118, 105119, 105120, 105121, 105122, 105123, 105124, 105125, 105150, 105151, 105152, 105153, 105154, 105155, 105156, 105157, 105158, 105159, 105160, 105161, 105162, 105163, 105164, 105165, 105166, 105167, 105168, 105169, 105170, 105171, 105172, 105173, 105174, 105175, 105200, 105201, 105202, 105203, 105204, 105205, 105206, 105207, 105208, 105209, 105210, 105211, 105212, 105213, 105214, 105215, 105216, 105217, 105218, 105219, 105220, 105221, 105222, 105223, 105224, 105225, 105250, 105251, 105252, 105253, 105254, 105255, 105256, 105257, 105258, 105259, 105260, 105261, 105262, 105263, 105264, 105265, 105266, 105267, 105268, 105269, 105270, 105271, 105272, 105273, 105274, 105275, 105300, 105301, 105302, 105303, 105304, 105305, 105306, 105307, 105308, 105309, 105310, 105311, 105312, 105313, 105314, 105315, 105316, 105317, 105318, 105319, 105320, 105321, 105322, 105323, 105324, 105325, 390611, 390616, 500470, 1490010, 1490110, 1491010, 1491110, 1492002, 1492101, 1493010, 1493110, 1493210, 1493221, 1494010, 1494107, 1494110, 1495100, 1495101, 1497203, 1498002, 1498011, 1630001, 1630101, 1631001, 1631200, 1631300, 1632002, 1632012, 1632101, 1636011, 1721001, 1721002, 1725201, 1726001, 1726002, 1728302, 1729220, 7022311, 7022316, 20010758, 20010781, 20010784, 20010786, 20013522, 20022100, 20381263, 20500912, 21491010, 21492010, 21493010, 21494010, 21630001, 21630003, 21630011, 21630013, 21668010, 21668011 };

    // 3: Blight (stateInfo 117)
    static const int status_3_ids[] = { 70 };

    // 4: Frost (stateInfo 260)
    static const int status_4_ids[] = { 829, 875, 880, 881, 1530, 1575, 1576, 1724, 1800, 1801, 1809, 1894, 1901, 1962, 3141, 3143, 3145, 3147, 3300, 3309, 6700, 6701, 6702, 6703, 6704, 6705, 6706, 24000, 24001, 24002, 24003, 24004, 24005, 24006, 24007, 24008, 24009, 24010, 24011, 24012, 24013, 24014, 24015, 24016, 24017, 24018, 24019, 24130, 24131, 24132, 24140, 24150, 24160, 24170, 107500, 107501, 107502, 107503, 107504, 107505, 107506, 107507, 107508, 107509, 107510, 107511, 107512, 107513, 107514, 107515, 107516, 107517, 107518, 107519, 107520, 107521, 107522, 107523, 107524, 107525, 107550, 107551, 107552, 107553, 107554, 107555, 107556, 107557, 107558, 107559, 107560, 107561, 107562, 107563, 107564, 107565, 107566, 107567, 107568, 107569, 107570, 107571, 107572, 107573, 107574, 107575, 107600, 107601, 107602, 107603, 107604, 107605, 107606, 107607, 107608, 107609, 107610, 107611, 107612, 107613, 107614, 107615, 107616, 107617, 107618, 107619, 107620, 107621, 107622, 107623, 107624, 107625, 107650, 107651, 107652, 107653, 107654, 107655, 107656, 107657, 107658, 107659, 107660, 107661, 107662, 107663, 107664, 107665, 107666, 107667, 107668, 107669, 107670, 107671, 107672, 107673, 107674, 107675, 107700, 107701, 107702, 107703, 107704, 107705, 107706, 107707, 107708, 107709, 107710, 107711, 107712, 107713, 107714, 107715, 107716, 107717, 107718, 107719, 107720, 107721, 107722, 107723, 107724, 107725, 107750, 107751, 107752, 107753, 107754, 107755, 107756, 107757, 107758, 107759, 107760, 107761, 107762, 107763, 107764, 107765, 107766, 107767, 107768, 107769, 107770, 107771, 107772, 107773, 107774, 107775, 107800, 107801, 107802, 107803, 107804, 107805, 107806, 107807, 107808, 107809, 107810, 107811, 107812, 107813, 107814, 107815, 107816, 107817, 107818, 107819, 107820, 107821, 107822, 107823, 107824, 107825, 500360, 500480, 1436100, 1437000, 1439000, 1439005, 1440000, 1440200, 1440205, 1441000, 1441610, 1442000, 1443100, 1443101, 1443200, 1449200, 1449300, 1449400, 1449500, 1449501, 1449502, 1449503, 1449504, 1452000, 1453000, 1453100, 1454000, 1454001, 1454002, 1455000, 1456000, 1459001, 1459011, 1464050, 1500000, 1500100, 1500200, 1501000, 1501001, 1506010, 1510000, 1511000, 1511005, 1515510, 1692100, 1702000, 1702100, 20000874, 20000884, 20001090, 20001091, 20001092, 20024130, 20024131, 20024132, 20381264, 20500360, 21500000, 21500001, 21620000, 21621000, 21621001, 21621002, 21621003, 21622000, 21623000, 21702000 };

    // 5: Sleep (stateInfo 436)
    static const int status_5_ids[] = { 1745, 1884, 1885, 3151, 3153, 3155, 3157, 6450, 6451, 6452, 6453, 6454, 6455, 6456, 6460, 19950, 19951, 19952, 19953, 19954, 25000, 25001, 25002, 25003, 25004, 25005, 25006, 25007, 25008, 25009, 25010, 25011, 25012, 25013, 25014, 25015, 25016, 25017, 25018, 25019, 102311, 102313, 102315, 102317, 105500, 105501, 105502, 105503, 105504, 105505, 105506, 105507, 105508, 105509, 105510, 105511, 105512, 105513, 105514, 105515, 105516, 105517, 105518, 105519, 105520, 105521, 105522, 105523, 105524, 105525, 105550, 105551, 105552, 105553, 105554, 105555, 105556, 105557, 105558, 105559, 105560, 105561, 105562, 105563, 105564, 105565, 105566, 105567, 105568, 105569, 105570, 105571, 105572, 105573, 105574, 105575, 105600, 105601, 105602, 105603, 105604, 105605, 105606, 105607, 105608, 105609, 105610, 105611, 105612, 105613, 105614, 105615, 105616, 105617, 105618, 105619, 105620, 105621, 105622, 105623, 105624, 105625, 105650, 105651, 105652, 105653, 105654, 105655, 105656, 105657, 105658, 105659, 105660, 105661, 105662, 105663, 105664, 105665, 105666, 105667, 105668, 105669, 105670, 105671, 105672, 105673, 105674, 105675, 105700, 105701, 105702, 105703, 105704, 105705, 105706, 105707, 105708, 105709, 105710, 105711, 105712, 105713, 105714, 105715, 105716, 105717, 105718, 105719, 105720, 105721, 105722, 105723, 105724, 105725, 105750, 105751, 105752, 105753, 105754, 105755, 105756, 105757, 105758, 105759, 105760, 105761, 105762, 105763, 105764, 105765, 105766, 105767, 105768, 105769, 105770, 105771, 105772, 105773, 105774, 105775, 500640, 1740000, 1742201, 1742251, 1749000, 1749200, 20001029, 20001034, 20001035, 20001039, 20001042, 20025000, 20025001, 20381265, 20500740, 20500911 };

    // 6: Madness (stateInfo 437)
    static const int status_6_ids[] = { 1780, 1781, 1782, 6750, 6751, 6752, 6753, 6754, 6755, 6756, 6757, 6758, 6775, 26000, 26001, 26002, 26003, 26004, 26005, 26006, 26007, 26008, 26009, 26010, 26011, 26012, 26013, 26014, 26015, 26016, 26017, 26018, 26019, 26100, 26110, 503314, 1730000, 1730001, 1730002, 1730100, 1730101, 1731000, 1731001, 1731002, 1731003, 1731100, 1731101, 1732000, 1732001, 1732002, 1732003, 1732010, 1732011, 1732020, 1732021, 1732025, 1732100, 1732101, 1732205, 1732300, 1732301, 1733001, 1733002, 1733100, 1733105, 1733200, 1733201, 1733202, 1733205, 1733206, 1735001, 1735105, 1736000, 1736100, 1736101, 1736102, 1737001, 1737005, 1738002, 1738101, 1738125, 1739002, 1739004, 1739005, 1739006, 1739007, 1739009, 6540002, 20000802, 20000821, 20381266, 20500620, 20500710, 21730000, 21730001, 21730002, 21731000, 21731001, 21732000, 21732001, 21732002, 21732003, 21733000, 21733001 };

    static int mapSpEffectToStatus(int id)
    {
        // Fast path for vanilla standard proc IDs
        if (id >= 2000 && id <= 2005) return 0; // Poison
        if (id >= 2010 && id <= 2012) return 1; // Rot
        if (id == 2020) return 2;               // Bleed
        if (id == 2030 || id == 70) return 3;   // Blight
        if (id >= 2040 && id <= 2041) return 4; // Frost
        if (id >= 2050 && id <= 2051) return 5; // Sleep
        if (id >= 2060 && id <= 2061) return 6; // Madness

        if (std::binary_search(std::begin(status_0_ids), std::end(status_0_ids), id)) return 0;
        if (std::binary_search(std::begin(status_1_ids), std::end(status_1_ids), id)) return 1;
        if (std::binary_search(std::begin(status_2_ids), std::end(status_2_ids), id)) return 2;
        if (std::binary_search(std::begin(status_3_ids), std::end(status_3_ids), id)) return 3;
        if (std::binary_search(std::begin(status_4_ids), std::end(status_4_ids), id)) return 4;
        if (std::binary_search(std::begin(status_5_ids), std::end(status_5_ids), id)) return 5;
        if (std::binary_search(std::begin(status_6_ids), std::end(status_6_ids), id)) return 6;

        return -1;
    }

    static void checkActiveSpEffects(GameData::ChrIns* chrIns, bool activeStatus[7], float activeTimer[7], int* outSpCount = nullptr, int* outSpIds = nullptr)
    {
        if (outSpCount) *outSpCount = 0;
        if (!chrIns || !isSafeReadable((uintptr_t)chrIns)) return;

        // SpecialEffectManager is at chrIns + 0x178
        uintptr_t chrAddr = (uintptr_t)chrIns;
            if (!isSafeReadable(chrAddr + 0x178)) return;

            uintptr_t spMgr = *(uintptr_t*)(chrAddr + 0x178);
            if (!isSafeReadable(spMgr)) return;
            if (!isSafeReadable(spMgr + 0x8)) return;

            uintptr_t node = *(uintptr_t*)(spMgr + 0x8);
            if (!node || node == spMgr) return;

            int safety = 0;
            int debugCount = 0;
            while (isSafeReadable(node) && node != spMgr && safety++ < 60)
            {
                if (!isSafeReadable(node + 0x60) || !isSafeReadable(node + 0x30))
                    break;

                // Engine check: test dword ptr [node + 0x60], 0x800c0003 -> skip inactive/deleted
                uint32_t flags = *(uint32_t*)(node + 0x60);
                if (flags & 0x800c0003)
                {
                    uintptr_t nextNode = *(uintptr_t*)(node + 0x30);
                    if (nextNode == node) break;
                    node = nextNode;
                    continue;
                }

                int id = 0;
                if (isSafeReadable(node + 0x8))
                    id = *(int*)(node + 0x8);

                float remainingDuration = 0.0f;
                if (isSafeReadable(node + 0x44))
                    remainingDuration = *(float*)(node + 0x44);

                if (outSpIds && debugCount < 6)
                {
                    outSpIds[debugCount++] = id;
                }

                int statusIdx = -1;

                // 1. Direct stateInfo check from engine SpEffectParam (the definitive truth for 100% of procs)
                uintptr_t paramPtr = *(uintptr_t*)node;
                if (isSafeReadable(paramPtr) && isSafeReadable(paramPtr + 0x158))
                {
                    uint16_t stateInfo = *(uint16_t*)(paramPtr + 0x156);
                    switch (stateInfo)
                    {
                    case 2:   statusIdx = 0; break; // Poison
                    case 5:   statusIdx = 1; break; // Rot
                    case 6:   statusIdx = 2; break; // Bleed
                    case 117: statusIdx = 3; break; // Blight
                    case 260: statusIdx = 4; break; // Frost
                    case 436: statusIdx = 5; break; // Sleep
                    case 437: statusIdx = 6; break; // Madness
                    default: break;
                    }
                }

                // 2. Fallback to ID map if stateInfo was not one of standard 7
                if (statusIdx < 0 && id > 0)
                {
                    statusIdx = mapSpEffectToStatus(id);
                }

                if (statusIdx >= 0 && statusIdx < 7)
                {
                    activeStatus[statusIdx] = true;
                    if (remainingDuration > 0.0f && remainingDuration < 600.0f)
                    {
                        if (remainingDuration > activeTimer[statusIdx])
                            activeTimer[statusIdx] = remainingDuration;
                    }
                    else if (activeTimer[statusIdx] < 1.0f)
                    {
                        activeTimer[statusIdx] = (statusIdx == 0 || statusIdx == 1 || statusIdx == 4) ? 30.0f : (statusIdx == 5 ? 15.0f : 4.0f);
                    }
                }

                uintptr_t nextNode = *(uintptr_t*)(node + 0x30);
                if (nextNode == node) break;
                node = nextNode;
            }

            if (outSpCount)
            {
                *outSpCount = debugCount;
            }
    }

    void PostureBarUI::updateUIBarStructs(uintptr_t moveMapStep, uintptr_t time)
    {
        if (g_postureUI && g_postureUI->updateUIBarStructsOriginal)
            g_postureUI->updateUIBarStructsOriginal(moveMapStep, time);

        if (isMenuOpen())
            return;

        auto&& worldChar = (GameData::WorldChrMan*)RPM<uintptr_t>(g_Hooking->worldChrSignature);
        if (!worldChar || !isSafeReadable((uintptr_t)worldChar))
            return;

        uintptr_t playerChrPtr = RPM<uintptr_t>((uintptr_t)worldChar + 0x1E508);
        if (playerChrPtr <= 0x10000 || playerChrPtr >= 0x7FFFFFFFFFFF || !isSafeReadable(playerChrPtr))
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            g_postureUI->playerPostureBar = std::nullopt;
            for (auto& b : g_postureUI->bossPostureBars) b = std::nullopt;
            for (auto& e : g_postureUI->entityPostureBars) e = std::nullopt;
            return;
        }

        auto&& feMan = (GameData::CSFeManImp*)RPM<uintptr_t>(g_Hooking->CSFeManSignature);
        if (!feMan || !isSafeReadable((uintptr_t)feMan) || !g_Hooking->GetChrInsFromHandleFunc)
            return;

        std::lock_guard<std::mutex> lock(dataMutex);

        if (PlayerPostureBarData::drawBar)
        {
            GameData::ChrIns* chrIns = nullptr;
            if (worldChar)
            {
                uintptr_t playerChrPtr = RPM<uintptr_t>((uintptr_t)worldChar + 0x1E508);
                if (playerChrPtr > 0x10000 && playerChrPtr < 0x7FFFFFFFFFFF)
                    chrIns = (GameData::ChrIns*)playerChrPtr;
            }

            if (chrIns && isSafeReadable((uintptr_t)chrIns) && isSafeReadable((uintptr_t)chrIns->chrModulelBag) && chrIns->chrModulelBag->staggerModule && isSafeReadable((uintptr_t)chrIns->chrModulelBag->staggerModule) && chrIns->chrModulelBag->staggerModule->staggerMax > 0.0f)
            {
                auto&& previousPlayerPostureBarData = g_postureUI->playerPostureBar;

                auto&& timePoint = std::chrono::steady_clock::now();

                PlayerPostureBarData playerPostureBarData;

                playerPostureBarData.maxStagger = chrIns->chrModulelBag->staggerModule->staggerMax;
                playerPostureBarData.stagger = chrIns->chrModulelBag->staggerModule->stagger;

                if (previousPlayerPostureBarData)
                {
                    // if previous value was below 0 and new value was reset
                    if (PlayerPostureBarData::resetStaggerTotalTime > 0.0f && previousPlayerPostureBarData->previousStagger <= 0.0f && playerPostureBarData.stagger == playerPostureBarData.maxStagger)
                    {
                        playerPostureBarData.resetStaggerTimer = PlayerPostureBarData::resetStaggerTotalTime;
                        playerPostureBarData.lastTimePoint = timePoint;
                        playerPostureBarData.isResetStagger = true;
                    }
                    else if (previousPlayerPostureBarData->isResetStagger)
                    {
                        playerPostureBarData.resetStaggerTimer = previousPlayerPostureBarData->resetStaggerTimer - std::chrono::duration_cast<std::chrono::duration<float>>(timePoint - previousPlayerPostureBarData->lastTimePoint).count();
                        playerPostureBarData.lastTimePoint = timePoint;
                        playerPostureBarData.isResetStagger = playerPostureBarData.resetStaggerTimer > 0.0f;
                    }

                    playerPostureBarData.previousStagger = playerPostureBarData.stagger;
                }

#ifdef DEBUGLOG
                playerPostureBarData.LogDebug();
#endif
                g_postureUI->playerPostureBar = playerPostureBarData;
            }
            else
            {
                g_postureUI->playerPostureBar = std::nullopt;
            }
        }

        for (IndexType i = 0; i < BOSS_CHR_ARRAY_LEN; i++)
        {
            if (feMan->bossHpBars[i].displayId < 0 || feMan->bossHpBars[i].displayId > 100)
            {
                g_postureUI->bossPostureBars[i] = std::nullopt;
                continue;
            }

            auto&& entityHandle = feMan->bossHpBars[i].bossHandle;
            if (entityHandle == 0 || entityHandle == __UINT64_MAX__)
            {
                g_postureUI->bossPostureBars[i] = std::nullopt;
                continue;
            }

            auto&& chrIns = g_Hooking->GetChrInsFromHandleFunc(worldChar, &entityHandle);
            auto&& previousBossPostureBarData = g_postureUI->bossPostureBars[i];

            if (!chrIns || !isSafeReadable((uintptr_t)chrIns) || !isSafeReadable((uintptr_t)chrIns->chrModulelBag))
            {
                g_postureUI->bossPostureBars[i] = std::nullopt;
                continue;
            }

                auto&& timePoint = std::chrono::steady_clock::now();

                BossPostureBarData bossPostureBarData;

                bossPostureBarData.entityHandle = entityHandle;
                bossPostureBarData.displayId = feMan->bossHpBars[i].displayId;
                bossPostureBarData.isStamina = BossPostureBarData::useStaminaForNPC && chrIns->modelNumber == 0;
                bossPostureBarData.isVisible = chrIns->chrModulelBag->statModule ? (chrIns->chrModulelBag->statModule->health > 0) : true;

                bool hasStagger = chrIns->chrModulelBag->staggerModule && (chrIns->chrModulelBag->staggerModule->staggerMax > 0.0f);
                bool hasStamina = chrIns->chrModulelBag->statModule && (chrIns->chrModulelBag->statModule->staminaMax > 0.0f);

                if (hasStagger)
                {
                    bossPostureBarData.hasValidBar = true;
                    bossPostureBarData.barDatas[EERDataType::Stagger].SetValue(chrIns->chrModulelBag->staggerModule->stagger, chrIns->chrModulelBag->staggerModule->staggerMax);
                }
                if (hasStamina)
                {
                    bossPostureBarData.barDatas[EERDataType::Stamina].SetValue(chrIns->chrModulelBag->statModule->stamina, chrIns->chrModulelBag->statModule->staminaMax);
                    if (bossPostureBarData.isStamina)
                        bossPostureBarData.hasValidBar = true;
                }

                bossPostureBarData.lastTimePoint = timePoint;
                if (StatusIconConfig::drawStatusIcons)
                {
                    auto* rm = getValidResistanceModule(chrIns->chrModulelBag);
                    if (rm && isSafeMemory((uintptr_t)rm))
                    {
                        int curRes[7] = { rm->poison, rm->rot, rm->bleed, rm->blight, rm->frost, rm->sleep, rm->madness };
                        int maxRes[7] = { rm->poisonMax, rm->rotMax, rm->bleedMax, rm->blightMax, rm->frostMax, rm->sleepMax, rm->madnessMax };

                        float dt = 0.016f;
                        if (previousBossPostureBarData && previousBossPostureBarData->entityHandle == entityHandle)
                        {
                            dt = std::chrono::duration_cast<std::chrono::duration<float>>(timePoint - previousBossPostureBarData->lastTimePoint).count();
                            if (dt <= 0.0f || dt > 1.0f) dt = 0.016f;
                        }

                        for (int s = 0; s < 7; s++)
                        {
                            float timer = 0.0f;
                            int prevVal = -1;
                            if (previousBossPostureBarData && previousBossPostureBarData->entityHandle == entityHandle)
                            {
                                timer = previousBossPostureBarData->statusActiveTimer[s] - dt;
                                if (timer < 0.0f) timer = 0.0f;
                                prevVal = previousBossPostureBarData->previousResistance[s];
                            }

                            int curVal = curRes[s];
                            int maxVal = maxRes[s];

                            if (maxVal > 0)
                            {
                                bool isProcced = (prevVal > 0 && curVal <= 0) || (curVal >= maxVal && prevVal > 0 && prevVal < maxVal);
                                bool justReset = (prevVal >= (int)(maxVal * 0.85f) && curVal <= (int)(maxVal * 0.15f)) ||
                                                 (prevVal > 0 && prevVal <= (int)(maxVal * 0.25f) && curVal >= (int)(maxVal * 0.75f));

                                if (isProcced || justReset)
                                {
                                    float duration = (s == 0 || s == 1 || s == 4) ? 30.0f : (s == 5 ? 15.0f : 4.0f);
                                    if (timer < 1.0f)
                                        timer = duration;
                                }
                            }

                            bossPostureBarData.statusActiveTimer[s] = timer;
                            bossPostureBarData.statusIsActive[s] = (timer > 0.0f);
                            bossPostureBarData.previousResistance[s] = curVal;
                        }
                    }

                    // Check SpEffect linked list
                    checkActiveSpEffects(chrIns, bossPostureBarData.statusIsActive, bossPostureBarData.statusActiveTimer, &bossPostureBarData.debugSpCount, bossPostureBarData.debugSpIds);
                }

                if (previousBossPostureBarData && previousBossPostureBarData->entityHandle == entityHandle)
                {
                    // if previous value was below 0 and new value was reset
                    if (BossPostureBarData::resetStaggerTotalTime > 0.0f && previousBossPostureBarData->previousStagger <= 0.0f && bossPostureBarData.barDatas[EERDataType::Stagger].IsValueMax())
                    {
                        bossPostureBarData.resetStaggerTimer = BossPostureBarData::resetStaggerTotalTime;
                        bossPostureBarData.lastTimePoint = timePoint;
                        bossPostureBarData.isResetStagger = true;
                    }
                    else if (previousBossPostureBarData->isResetStagger)
                    {
                        bossPostureBarData.resetStaggerTimer = previousBossPostureBarData->resetStaggerTimer - std::chrono::duration_cast<std::chrono::duration<float>>(timePoint - previousBossPostureBarData->lastTimePoint).count();
                        bossPostureBarData.lastTimePoint = timePoint;
                        bossPostureBarData.isResetStagger = bossPostureBarData.resetStaggerTimer > 0.0f;
                    }

                    bossPostureBarData.previousStagger = bossPostureBarData.barDatas[EERDataType::Stagger].value;
                }

#ifdef DEBUGLOG
                bossPostureBarData.LogDebug();
#endif
                g_postureUI->bossPostureBars[i] = bossPostureBarData;
        }


        for (IndexType i = 0; i < ENTITY_CHR_ARRAY_LEN; i++)
        {
            if (!feMan->entityHpBars[i].isVisible)
            {
                g_postureUI->entityPostureBars[i] = std::nullopt;
                continue;
            }

            auto&& entityHandle = feMan->entityHpBars[i].entityHandle;
            if (entityHandle == 0 || entityHandle == __UINT64_MAX__)
            {
                g_postureUI->entityPostureBars[i] = std::nullopt;
                continue;
            }

            auto&& chrIns = g_Hooking->GetChrInsFromHandleFunc(worldChar, &entityHandle);
            auto&& previousEntityPostureBarData = g_postureUI->entityPostureBars[i];

            if (!chrIns || !isSafeReadable((uintptr_t)chrIns) || !isSafeReadable((uintptr_t)chrIns->chrModulelBag))
            {
                g_postureUI->entityPostureBars[i] = std::nullopt;
                continue;
            }

                auto&& timePoint = std::chrono::steady_clock::now();

                EntityPostureBarData entityPostureBarData;

                entityPostureBarData.entityHandle = entityHandle;
                entityPostureBarData.isStamina = EntityPostureBarData::useStaminaForNPC && chrIns->modelNumber == 0;
                entityPostureBarData.screenX = feMan->entityHpBars[i].screenPosX;
                entityPostureBarData.screenY = feMan->entityHpBars[i].screenPosY;
                entityPostureBarData.distanceModifier = feMan->entityHpBars[i].mod;
                entityPostureBarData.isVisible = feMan->entityHpBars[i].isVisible && (chrIns->chrModulelBag->statModule ? chrIns->chrModulelBag->statModule->health > 0 : true);

                bool hasStagger = chrIns->chrModulelBag->staggerModule && (chrIns->chrModulelBag->staggerModule->staggerMax > 0.0f);
                bool hasStamina = chrIns->chrModulelBag->statModule && (chrIns->chrModulelBag->statModule->staminaMax > 0.0f);

                if (hasStagger)
                {
                    entityPostureBarData.hasValidBar = true;
                    entityPostureBarData.barDatas[EERDataType::Stagger].SetValue(chrIns->chrModulelBag->staggerModule->stagger, chrIns->chrModulelBag->staggerModule->staggerMax);
                }
                if (hasStamina)
                {
                    entityPostureBarData.barDatas[EERDataType::Stamina].SetValue(chrIns->chrModulelBag->statModule->stamina, chrIns->chrModulelBag->statModule->staminaMax);
                    if (entityPostureBarData.isStamina)
                        entityPostureBarData.hasValidBar = true;
                }

                entityPostureBarData.lastTimePoint = timePoint;
                if (StatusIconConfig::drawStatusIcons)
                {
                    auto* rm = getValidResistanceModule(chrIns->chrModulelBag);
                    if (rm && isSafeMemory((uintptr_t)rm))
                    {
                        int curRes[7] = { rm->poison, rm->rot, rm->bleed, rm->blight, rm->frost, rm->sleep, rm->madness };
                        int maxRes[7] = { rm->poisonMax, rm->rotMax, rm->bleedMax, rm->blightMax, rm->frostMax, rm->sleepMax, rm->madnessMax };

                        float dt = 0.016f;
                        if (previousEntityPostureBarData && previousEntityPostureBarData->entityHandle == entityHandle)
                        {
                            dt = std::chrono::duration_cast<std::chrono::duration<float>>(timePoint - previousEntityPostureBarData->lastTimePoint).count();
                            if (dt <= 0.0f || dt > 1.0f) dt = 0.016f;
                        }

                        for (int s = 0; s < 7; s++)
                        {
                            float timer = 0.0f;
                            int prevVal = -1;
                            if (previousEntityPostureBarData && previousEntityPostureBarData->entityHandle == entityHandle)
                            {
                                timer = previousEntityPostureBarData->statusActiveTimer[s] - dt;
                                if (timer < 0.0f) timer = 0.0f;
                                prevVal = previousEntityPostureBarData->previousResistance[s];
                            }

                            int curVal = curRes[s];
                            int maxVal = maxRes[s];

                            if (maxVal > 0)
                            {
                                bool isProcced = (prevVal > 0 && curVal <= 0) || (curVal >= maxVal && prevVal > 0 && prevVal < maxVal);
                                bool justReset = (prevVal >= (int)(maxVal * 0.85f) && curVal <= (int)(maxVal * 0.15f)) ||
                                                 (prevVal > 0 && prevVal <= (int)(maxVal * 0.25f) && curVal >= (int)(maxVal * 0.75f));

                                if (isProcced || justReset)
                                {
                                    float duration = (s == 0 || s == 1 || s == 4) ? 30.0f : (s == 5 ? 15.0f : 4.0f);
                                    if (timer < 1.0f)
                                        timer = duration;
                                }
                            }

                            entityPostureBarData.statusActiveTimer[s] = timer;
                            entityPostureBarData.statusIsActive[s] = (timer > 0.0f);
                            entityPostureBarData.previousResistance[s] = curVal;
                        }
                    }

                    // Check SpEffect linked list
                    checkActiveSpEffects(chrIns, entityPostureBarData.statusIsActive, entityPostureBarData.statusActiveTimer, &entityPostureBarData.debugSpCount, entityPostureBarData.debugSpIds);
                }

                entityPostureBarData.gameUiUpdateTimePoint = timePoint;
                entityPostureBarData.gamePreviousUiUpdateTimePoint = timePoint;
                entityPostureBarData.previousScreenX = entityPostureBarData.screenX;
                entityPostureBarData.previousScreenY = entityPostureBarData.screenY;

                if (previousEntityPostureBarData && previousEntityPostureBarData->entityHandle == entityHandle)
                {
                    // if previous value was below 0 and new value was reset
                    if (EntityPostureBarData::resetStaggerTotalTime > 0.0f && previousEntityPostureBarData->previousStagger <= 0.0f && entityPostureBarData.barDatas[EERDataType::Stagger].IsValueMax())
                    {
                        entityPostureBarData.resetStaggerTimer = EntityPostureBarData::resetStaggerTotalTime;
                        entityPostureBarData.lastTimePoint = timePoint;
                        entityPostureBarData.isResetStagger = true;
                    }
                    else if (previousEntityPostureBarData->isResetStagger)
                    {
                        entityPostureBarData.resetStaggerTimer = previousEntityPostureBarData->resetStaggerTimer - std::chrono::duration_cast<std::chrono::duration<float>>(timePoint - previousEntityPostureBarData->lastTimePoint).count();
                        entityPostureBarData.lastTimePoint = timePoint;
                        entityPostureBarData.isResetStagger = entityPostureBarData.resetStaggerTimer > 0.0f;
                    }

                    entityPostureBarData.gamePreviousUiUpdateTimePoint = previousEntityPostureBarData->gameUiUpdateTimePoint;
                    entityPostureBarData.previousScreenX = previousEntityPostureBarData->screenX;
                    entityPostureBarData.previousScreenY = previousEntityPostureBarData->screenY;
                    entityPostureBarData.previousStagger = entityPostureBarData.barDatas[EERDataType::Stagger].value;
                }

#ifdef DEBUGLOG
                entityPostureBarData.LogDebug();
#endif
                g_postureUI->entityPostureBars[i] = entityPostureBarData;
        }

#ifdef DEBUGLOG
        }
        catch (const std::exception& e)
        {
            Logger::useLogger = true;
            Logger::log(e.what(), LogLevel::Error);
            throw;
        }
        catch (...)
        {
            Logger::useLogger = true;
            Logger::log("Unknown exception during PostureBarUI::updateUIBarStructs", LogLevel::Error);
            throw;
        }
#endif // DEBUGLOG
    }

#ifdef DEBUGLOG
    void PlayerPostureBarData::LogDebug()
    {
        Logger::log("---------------------------------------------------------------------------------");
        Logger::log("PlayerPostureBarData: ");
        Logger::log("\tmaxStagger: " + std::to_string(maxStagger));
        Logger::log("\tstagger: " + std::to_string(stagger));
        Logger::log("\tpreviousStagger: " + std::to_string(previousStagger));
        Logger::log("\tisResetStagger: " + std::to_string(isResetStagger));
        Logger::log("\tresetStaggerTimer: " + std::to_string(resetStaggerTimer));
        Logger::log("\tlastTimePoint: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(lastTimePoint.time_since_epoch()).count()));
        Logger::log("---------------------------------------------------------------------------------");
    }

    void BossPostureBarData::LogDebug()
    {
        Logger::log("---------------------------------------------------------------------------------");
        Logger::log("PlayerPostureBarData: ");
        Logger::log("\tentityHandle: " + std::to_string(entityHandle));
        Logger::log("\tdisplayId: " + std::to_string(displayId));
        Logger::log("\tisStamina: " + std::to_string(isStamina));
        Logger::log("\tisVisible: " + std::to_string(isVisible));
        Logger::log("\tbarDatas: ");
        for (std::pair<EERDataType, BarData> barEntry : barDatas)
        {
            Logger::log("\t\t" + to_string(barEntry.first) + ":");
            Logger::log("\t\tvalue:" + std::to_string(barEntry.second.value));
            Logger::log("\t\tmaxValue:" + std::to_string(barEntry.second.maxValue));
        }
        Logger::log("\tpreviousStagger: " + std::to_string(previousStagger));
        Logger::log("\tisResetStagger: " + std::to_string(isResetStagger));
        Logger::log("\tresetStaggerTimer: " + std::to_string(resetStaggerTimer));
        Logger::log("\tlastTimePoint: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(lastTimePoint.time_since_epoch()).count()));
        Logger::log("---------------------------------------------------------------------------------");
    }

    void EntityPostureBarData::LogDebug()
    {
        Logger::log("---------------------------------------------------------------------------------");
        Logger::log("PlayerPostureBarData: ");
        Logger::log("\tentityHandle: " + std::to_string(entityHandle));
        Logger::log("\tisStamina: " + std::to_string(isStamina));
        Logger::log("\tisVisible: " + std::to_string(isVisible));
        Logger::log("\tscreenX: " + std::to_string(screenX));
        Logger::log("\tscreenY: " + std::to_string(screenY));
        Logger::log("\tdistanceModifier: " + std::to_string(distanceModifier));
        Logger::log("\tpreviousScreenX: " + std::to_string(previousScreenX));
        Logger::log("\tpreviousScreenY: " + std::to_string(previousScreenY));
        Logger::log("\tgameUiUpdateTimePoint: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(gameUiUpdateTimePoint.time_since_epoch()).count()));
        Logger::log("\tgamePreviousUiUpdateTimePoint: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(gamePreviousUiUpdateTimePoint.time_since_epoch()).count()));
        Logger::log("\tbarDatas: ");
        for (std::pair<EERDataType, BarData> barEntry : barDatas)
        {
            Logger::log("\t\t" + to_string(barEntry.first) + ":");
            Logger::log("\t\tvalue:" + std::to_string(barEntry.second.value));
            Logger::log("\t\tmaxValue:" + std::to_string(barEntry.second.maxValue));
        }
        Logger::log("\tpreviousStagger: " + std::to_string(previousStagger));
        Logger::log("\tisResetStagger: " + std::to_string(isResetStagger));
        Logger::log("\tresetStaggerTimer: " + std::to_string(resetStaggerTimer));
        Logger::log("\tlastTimePoint: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(lastTimePoint.time_since_epoch()).count()));
        Logger::log("---------------------------------------------------------------------------------");
    }
#endif // DEBUGLOG

} // namespace ER

