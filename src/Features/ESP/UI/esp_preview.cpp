#include "app/Core/globals.h"
#include "Features/ESP/esp.h"

#include <algorithm>
#include <charconv>

#include <imgui.h>

namespace ui {
void RenderEspPreview();
}

namespace
{
    ImU32 Col4(const float* c)
    {
        return IM_COL32(
            static_cast<int>(c[0] * 255),
            static_cast<int>(c[1] * 255),
            static_cast<int>(c[2] * 255),
            static_cast<int>(c[3] * 255));
    }

    void TextShadow(ImDrawList* dl, const ImVec2& pos, ImU32 color, const char* text)
    {
        dl->AddText(ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 220), text);
        dl->AddText(pos, color, text);
    }

    void TextShadowFont(ImDrawList* dl, ImFont* font, float size,
                        const ImVec2& pos, ImU32 color, const char* text)
    {
        if (!font) { TextShadow(dl, pos, color, text); return; }
        const int a = (color >> IM_COL32_A_SHIFT) & 0xFF;
        const int sa = a < 220 ? (220 * a / 255) : 220;
        dl->AddText(font, size, ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, sa), text);
        dl->AddText(font, size, pos, color, text);
    }

    void PrvCornerBox(ImDrawList* dl, float x, float y, float w, float h,
                      ImU32 color, float cornerLen, float thickness)
    {
        const ImU32 outline = IM_COL32(0, 0, 0, 220);
        float cl = cornerLen;
        if (cl > w * 0.5f) cl = w * 0.5f;
        if (cl > h * 0.5f) cl = h * 0.5f;
        const float ot = thickness + 2.0f;

        dl->AddLine(ImVec2(x - 1, y), ImVec2(x + cl, y), outline, ot);
        dl->AddLine(ImVec2(x, y - 1), ImVec2(x, y + cl), outline, ot);
        dl->AddLine(ImVec2(x + w - cl, y), ImVec2(x + w + 1, y), outline, ot);
        dl->AddLine(ImVec2(x + w, y - 1), ImVec2(x + w, y + cl), outline, ot);
        dl->AddLine(ImVec2(x - 1, y + h), ImVec2(x + cl, y + h), outline, ot);
        dl->AddLine(ImVec2(x, y + h - cl), ImVec2(x, y + h + 1), outline, ot);
        dl->AddLine(ImVec2(x + w - cl, y + h), ImVec2(x + w + 1, y + h), outline, ot);
        dl->AddLine(ImVec2(x + w, y + h - cl), ImVec2(x + w, y + h + 1), outline, ot);

        dl->AddLine(ImVec2(x, y), ImVec2(x + cl, y), color, thickness);
        dl->AddLine(ImVec2(x, y), ImVec2(x, y + cl), color, thickness);
        dl->AddLine(ImVec2(x + w - cl, y), ImVec2(x + w, y), color, thickness);
        dl->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + cl), color, thickness);
        dl->AddLine(ImVec2(x, y + h), ImVec2(x + cl, y + h), color, thickness);
        dl->AddLine(ImVec2(x, y + h - cl), ImVec2(x, y + h), color, thickness);
        dl->AddLine(ImVec2(x + w - cl, y + h), ImVec2(x + w, y + h), color, thickness);
        dl->AddLine(ImVec2(x + w, y + h - cl), ImVec2(x + w, y + h), color, thickness);
    }

    void PrvSideBar(ImDrawList* dl, float x, float top, float height,
                    float barW, float fraction, ImU32 fillCol)
    {
        const float visualBarW = 2.0f;
        const float barInset = std::max(0.0f, (barW - visualBarW) * 0.5f);
        const float barLeft = x + barInset;
        const float outlinePad = 0.75f;
        const float barH = height * fraction;
        dl->AddRectFilled(ImVec2(barLeft - outlinePad, top - outlinePad),
                          ImVec2(barLeft + visualBarW + outlinePad, top + height + outlinePad),
                          IM_COL32(10, 10, 10, 185), 1.5f);
        dl->AddRect(ImVec2(barLeft - outlinePad, top - outlinePad),
                    ImVec2(barLeft + visualBarW + outlinePad, top + height + outlinePad),
                    IM_COL32(0, 0, 0, 160), 1.5f, 0, 1.0f);
        if (fraction > 0.0f)
            dl->AddRectFilled(ImVec2(barLeft, top + height - barH),
                              ImVec2(barLeft + visualBarW, top + height), fillCol, 1.0f);
    }

    struct Vec2 { float x, y; };

    Vec2 BonePos2D(int boneId, float cx, float top, float scale)
    {
        float s = scale;
        switch (boneId) {
        case esp::HEAD:           return { cx,          top + 10*s  };
        case esp::NECK:           return { cx,          top + 22*s  };
        case esp::CHEST:          return { cx,          top + 38*s  };
        case esp::SPINE2:         return { cx,          top + 54*s  };
        case esp::SPINE1:         return { cx,          top + 70*s  };
        case esp::PELVIS:         return { cx,          top + 90*s  };
        case esp::SHOULDER_L:     return { cx + 18*s,   top + 26*s  };
        case esp::ELBOW_L:        return { cx + 32*s,   top + 52*s  };
        case esp::HAND_L:         return { cx + 34*s,   top + 70*s  };
        case esp::SHOULDER_R:     return { cx - 18*s,   top + 26*s  };
        case esp::ELBOW_R:        return { cx - 32*s,   top + 52*s  };
        case esp::HAND_R:         return { cx - 34*s,   top + 70*s  };
        case esp::HIP_L:          return { cx + 10*s,   top + 95*s  };
        case esp::KNEE_L:         return { cx + 13*s,   top + 125*s };
        case esp::FOOT_HEEL_L:    return { cx + 14*s,   top + 146*s };
        case esp::FOOT_TOES_L_T:
        case esp::FOOT_TOES_L_CT: return { cx + 16*s,   top + 154*s };
        case esp::HIP_R:          return { cx - 10*s,   top + 95*s  };
        case esp::KNEE_R:         return { cx - 13*s,   top + 125*s };
        case esp::FOOT_HEEL_R:    return { cx - 14*s,   top + 146*s };
        case esp::FOOT_TOES_R_T:
        case esp::FOOT_TOES_R_CT: return { cx - 16*s,   top + 154*s };
        default: return { cx,          top + 50*s  };
        }
    }

    struct BonePair { int from, to; };
    static const BonePair kPairs[] = {
        {esp::PELVIS,     esp::SPINE1},
        {esp::SPINE1,     esp::SPINE2},
        {esp::SPINE2,     esp::CHEST},
        {esp::CHEST,      esp::NECK},
        {esp::NECK,       esp::HEAD},
        {esp::NECK,       esp::SHOULDER_L},
        {esp::SHOULDER_L, esp::ELBOW_L},
        {esp::ELBOW_L,    esp::HAND_L},
        {esp::NECK,       esp::SHOULDER_R},
        {esp::SHOULDER_R, esp::ELBOW_R},
        {esp::ELBOW_R,    esp::HAND_R},
        {esp::PELVIS,     esp::HIP_L},
        {esp::HIP_L,      esp::KNEE_L},
        {esp::KNEE_L,     esp::FOOT_HEEL_L},
        {esp::FOOT_HEEL_L, esp::FOOT_TOES_L_CT},
        {esp::PELVIS,     esp::HIP_R},
        {esp::HIP_R,      esp::KNEE_R},
        {esp::KNEE_R,     esp::FOOT_HEEL_R},
        {esp::FOOT_HEEL_R, esp::FOOT_TOES_R_CT},
    };
    static const int kJoints[] = {
        esp::PELVIS,
        esp::SPINE1,
        esp::SPINE2,
        esp::CHEST,
        esp::NECK,
        esp::HEAD,
        esp::SHOULDER_L,
        esp::ELBOW_L,
        esp::HAND_L,
        esp::SHOULDER_R,
        esp::ELBOW_R,
        esp::HAND_R,
        esp::HIP_L,
        esp::KNEE_L,
        esp::FOOT_HEEL_L,
        esp::FOOT_TOES_L_CT,
        esp::HIP_R,
        esp::KNEE_R,
        esp::FOOT_HEEL_R,
        esp::FOOT_TOES_R_CT
    };

    
    void DrawPlayerSilhouette(ImDrawList* dl, float cx, float boxTop,
                              float boxW, float boxH, float boneScale,
                              ImU32 entityCol, bool showBottomLabels,
                              float areaBottom, bool useVisColors = true)
    {
        struct PreviewBarLabel {
            bool active = false;
            char text[8] = {};
            ImVec2 textPos = {};
            ImVec2 bgMin = {};
            ImVec2 bgMax = {};
            ImU32 textColor = 0;
            ImU32 accentColor = 0;
        };

        const float boxLeft = cx - boxW * 0.5f;
        const int mockHp = 72;
        const int mockArmor = 45;
        const float hpFrac = mockHp / 100.0f;
        const float apFrac = mockArmor / 100.0f;
        const float sideBarW = 3.0f;
        const float sideBarGap = 4.0f;

        
        if (g::espSnaplines && showBottomLabels) {
            float fromY = g::espSnaplineFromTop ? (boxTop - 30) : areaBottom;
            dl->AddLine(ImVec2(cx, fromY), ImVec2(cx, boxTop + boxH),
                        IM_COL32(0, 0, 0, 180), 2.5f);
            dl->AddLine(ImVec2(cx, fromY), ImVec2(cx, boxTop + boxH),
                        Col4(g::espSnaplineColor), 1.0f);
        }

        
        if (g::espBox)
            PrvCornerBox(dl, boxLeft, boxTop, boxW, boxH,
                         entityCol, boxH * 0.22f, 2.0f);

        
        const float healthBarLeft = boxLeft - sideBarW - sideBarGap;
        const float armorBarLeft = g::espHealth
            ? (healthBarLeft - sideBarW - sideBarGap) : healthBarLeft;
        const float visualBarWidth = 2.0f;
        const float barInset = std::max(0.0f, (sideBarW - visualBarWidth) * 0.5f);
        const float centeredHealthBarLeft = healthBarLeft + barInset;
        const float centeredArmorBarLeft = armorBarLeft + barInset;

        ImFont* barValueFont = ImGui::GetFont();
        const float barValueFontSize =
            barValueFont ? std::max(7.5f, ImGui::GetFontSize() - 4.5f) : 0.0f;
        auto calcBarValueSize = [&](const char* text) -> ImVec2 {
            if (barValueFont && barValueFontSize > 0.0f)
                return barValueFont->CalcTextSizeA(barValueFontSize, FLT_MAX, 0.0f, text, nullptr);
            return ImGui::CalcTextSize(text);
        };
        auto makeBarLabel = [&](PreviewBarLabel& label, int value, float barLeft, ImU32 textColor, ImU32 accentColor) {
            const auto result = std::to_chars(label.text, label.text + sizeof(label.text) - 1, value);
            *result.ptr = '\0';
            const ImVec2 textSize = calcBarValueSize(label.text);
            const float padX = 2.5f;
            const float padY = 1.0f;
            const float labelWidth = textSize.x + padX * 2.0f;
            const float labelHeight = textSize.y + padY * 2.0f;
            float bgX = (barLeft + visualBarWidth * 0.5f) - labelWidth * 0.5f;
            float bgY = boxTop - labelHeight - 3.0f;
            label.active = true;
            label.textPos = ImVec2(bgX + padX, bgY + padY - 0.25f);
            label.bgMin = ImVec2(bgX, bgY);
            label.bgMax = ImVec2(bgX + labelWidth, bgY + labelHeight);
            label.textColor = textColor;
            label.accentColor = accentColor;
        };
        auto shiftLabelX = [&](PreviewBarLabel& label, float deltaX) {
            label.bgMin.x += deltaX;
            label.bgMax.x += deltaX;
            label.textPos.x += deltaX;
        };
        auto shiftLabelY = [&](PreviewBarLabel& label, float deltaY) {
            label.bgMin.y += deltaY;
            label.bgMax.y += deltaY;
            label.textPos.y += deltaY;
        };
        auto setLabelX = [&](PreviewBarLabel& label, float bgX) {
            shiftLabelX(label, bgX - label.bgMin.x);
        };
        auto setLabelY = [&](PreviewBarLabel& label, float bgY) {
            shiftLabelY(label, bgY - label.bgMin.y);
        };
        auto labelWidth = [](const PreviewBarLabel& label) -> float {
            return label.bgMax.x - label.bgMin.x;
        };
        auto drawBarLabel = [&](const PreviewBarLabel& label) {
            if (!label.active || label.text[0] == '\0')
                return;
            dl->AddRectFilled(label.bgMin, label.bgMax, IM_COL32(8, 8, 8, 205), 2.5f);
            dl->AddRect(label.bgMin, label.bgMax, IM_COL32(0, 0, 0, 150), 2.5f, 0, 1.0f);
            dl->AddRectFilled(
                ImVec2(label.bgMin.x + 1.0f, label.bgMax.y - 2.0f),
                ImVec2(label.bgMax.x - 1.0f, label.bgMax.y - 1.0f),
                label.accentColor,
                1.0f);

            if (!barValueFont || barValueFontSize <= 0.0f) {
                dl->AddText(ImVec2(label.textPos.x + 1.0f, label.textPos.y + 1.0f), IM_COL32(0, 0, 0, 210), label.text);
                dl->AddText(label.textPos, label.textColor, label.text);
                return;
            }

            dl->AddText(barValueFont, barValueFontSize,
                        ImVec2(label.textPos.x + 1.0f, label.textPos.y + 1.0f),
                        IM_COL32(0, 0, 0, 210), label.text);
            dl->AddText(barValueFont, barValueFontSize, label.textPos, label.textColor, label.text);
        };
        PreviewBarLabel hpLabel = {};
        PreviewBarLabel apLabel = {};

        if (g::espArmor) {
            PrvSideBar(dl, armorBarLeft, boxTop, boxH, sideBarW, apFrac,
                       Col4(g::espArmorColor));
            if (g::espArmorText && mockArmor < 100)
                makeBarLabel(apLabel, mockArmor, centeredArmorBarLeft, IM_COL32(225, 245, 255, 255), Col4(g::espArmorColor));
        }
        if (g::espHealth) {
            PrvSideBar(dl, healthBarLeft, boxTop, boxH, sideBarW, hpFrac,
                       Col4(g::espHealthColor));
            if (g::espHealthText && mockHp < 100)
                makeBarLabel(hpLabel, mockHp, centeredHealthBarLeft, IM_COL32(255, 255, 255, 255), Col4(g::espHealthColor));
        }
        if (hpLabel.active && apLabel.active) {
            const float sharedLabelY = std::min(hpLabel.bgMin.y, apLabel.bgMin.y);
            setLabelY(hpLabel, sharedLabelY);
            setLabelY(apLabel, sharedLabelY);

            const float pairGap = 3.0f;
            const float totalWidth = labelWidth(apLabel) + pairGap + labelWidth(hpLabel);
            const float pairCenterX =
                ((centeredArmorBarLeft + visualBarWidth * 0.5f) + (centeredHealthBarLeft + visualBarWidth * 0.5f)) * 0.5f;
            setLabelX(apLabel, pairCenterX - totalWidth * 0.5f);
            setLabelX(hpLabel, apLabel.bgMax.x + pairGap);
        }
        drawBarLabel(hpLabel);
        drawBarLabel(apLabel);

        
        if (g::espSkeleton) {
            ImU32 skelCol = (g::espVisibilityColoring && useVisColors) ? entityCol : Col4(g::espSkeletonColor);
            for (auto& p : kPairs) {
                Vec2 a = BonePos2D(p.from, cx, boxTop, boneScale);
                Vec2 b = BonePos2D(p.to, cx, boxTop, boneScale);
                dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y),
                            IM_COL32(0, 0, 0, 200), 3.0f);
            }
            for (auto& p : kPairs) {
                Vec2 a = BonePos2D(p.from, cx, boxTop, boneScale);
                Vec2 b = BonePos2D(p.to, cx, boxTop, boneScale);
                dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), skelCol, 1.6f);
            }
            for (int j : kJoints) {
                Vec2 bp = BonePos2D(j, cx, boxTop, boneScale);
                dl->AddCircleFilled(ImVec2(bp.x, bp.y), 2.2f,
                                    IM_COL32(0, 0, 0, 200), 8);
                dl->AddCircleFilled(ImVec2(bp.x, bp.y), 1.4f, skelCol, 8);
            }
            Vec2 head = BonePos2D(esp::HEAD, cx, boxTop, boneScale);
            float headR = 7.0f * boneScale;
            dl->AddCircle(ImVec2(head.x, head.y), headR,
                          IM_COL32(0, 0, 0, 200), 16, 2.8f);
            dl->AddCircle(ImVec2(head.x, head.y), headR, skelCol, 16, 1.4f);
        }

        
        if (g::espName) {
            const char* name = "KevQ";
            ImFont* font = g::fontSegoeBold ? g::fontSegoeBold : ImGui::GetFont();
            float fontSize = g::espNameFontSize > 0 ? g::espNameFontSize : ImGui::GetFontSize();
            ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, name);
            TextShadowFont(dl, font, fontSize,
                           ImVec2(cx - ts.x * 0.5f, boxTop - ts.y - 4),
                           Col4(g::espNameColor), name);
        }

        
        {
            float bottomY = boxTop + boxH + 4;
            if (g::espWeapon) {
                
                if (g::fontWeaponIcons && g::espWeaponIcon) {
                    const char* icon = "W"; 
                    const float iconSize = g::espWeaponIconSize;
                    ImVec2 its = g::fontWeaponIcons->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, icon);
                    TextShadowFont(dl, g::fontWeaponIcons, iconSize,
                                   ImVec2(cx - its.x * 0.5f, bottomY),
                                   Col4(g::espWeaponIconColor), icon);
                    bottomY += its.y + 1;
                }
                
                if (g::espWeaponText) {
                    const char* weapon = "AK-47";
                    ImFont* tf = ImGui::GetFont();
                    float tfs = g::espWeaponTextSize > 0.0f ? g::espWeaponTextSize : ImGui::GetFontSize();
                    ImVec2 ts = tf->CalcTextSizeA(tfs, FLT_MAX, 0.0f, weapon);
                    TextShadowFont(dl, tf, tfs, ImVec2(cx - ts.x * 0.5f, bottomY),
                                   Col4(g::espWeaponTextColor), weapon);
                    bottomY += ts.y + 2;
                }
            }
            if (g::espWeaponAmmo) {
                const char* ammo = "25 / 30";
                ImFont* af = ImGui::GetFont();
                float afs = g::espWeaponAmmoSize > 0.0f ? g::espWeaponAmmoSize : ImGui::GetFontSize();
                ImVec2 ts = af->CalcTextSizeA(afs, FLT_MAX, 0.0f, ammo);
                TextShadowFont(dl, af, afs, ImVec2(cx - ts.x * 0.5f, bottomY),
                               Col4(g::espWeaponAmmoColor), ammo);
            }
        }


        
        if (g::espFlags) {
            float flagY = boxTop;
            float flagX = boxLeft + boxW + 6;
            if (g::espFlagScoped) {
                TextShadow(dl, ImVec2(flagX, flagY), Col4(g::espFlagScopedColor), "Scoped");
                flagY += ImGui::GetFontSize() + 1;
            }
            if (g::espFlagMoney) {
                TextShadow(dl, ImVec2(flagX, flagY), Col4(g::espFlagMoneyColor), "$4200");
                flagY += ImGui::GetFontSize() + 1;
            }
            if (g::espDistance) {
                TextShadow(dl, ImVec2(flagX, flagY), Col4(g::espDistanceColor), "42m");
                flagY += ImGui::GetFontSize() + 1;
            }
        }
    }
}

void ui::RenderEspPreview()
{
    if (!g::espPreviewOpen || !g::espEnabled)
        return;

    const bool triMode = g::espVisibilityColoring;
    const float defaultW = triMode ? 560.0f : 280.0f;

    ImGui::SetNextWindowSize(ImVec2(defaultW, 380), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(triMode ? 480.0f : 240.0f, 320),
        ImVec2(triMode ? 780.0f : 400.0f, 520));

    if (!ImGui::Begin("ESP Preview", &g::espPreviewOpen,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {
        ImGui::End();
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 winPos = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float pw = avail.x;
    const float ph = avail.y;

    
    dl->AddRectFilled(winPos, ImVec2(winPos.x + pw, winPos.y + ph),
                      IM_COL32(12, 12, 12, 255), 4.0f);

    if (triMode) {
        
        const float thirdW = pw / 3.0f;
        const float boxH = ph * 0.36f;
        const float boxW = boxH * 0.48f;
        const float boneScale = boxH / 160.0f;
        const float boxTop = winPos.y + ph * 0.16f;

        
        float div1 = winPos.x + thirdW;
        float div2 = winPos.x + thirdW * 2.0f;
        dl->AddLine(ImVec2(div1, winPos.y + 4), ImVec2(div1, winPos.y + ph - 4),
                    IM_COL32(50, 50, 50, 180), 1.0f);
        dl->AddLine(ImVec2(div2, winPos.y + 4), ImVec2(div2, winPos.y + ph - 4),
                    IM_COL32(50, 50, 50, 180), 1.0f);

        
        float cx1 = winPos.x + thirdW * 0.5f;
        ImU32 defCol = Col4(g::espBoxColor);
        DrawPlayerSilhouette(dl, cx1, boxTop, boxW, boxH, boneScale,
                             defCol, true, winPos.y + ph - 22, false);

        
        float cx2 = winPos.x + thirdW * 1.5f;
        ImU32 visCol = Col4(g::espVisibleColor);
        DrawPlayerSilhouette(dl, cx2, boxTop, boxW, boxH, boneScale,
                             visCol, true, winPos.y + ph - 22);

        
        float cx3 = winPos.x + thirdW * 2.5f;
        ImU32 hidCol = Col4(g::espHiddenColor);
        DrawPlayerSilhouette(dl, cx3, boxTop, boxW, boxH, boneScale,
                             hidCol, true, winPos.y + ph - 22);

        
        const char* defLabel = "Default";
        const char* visLabel = "Visible";
        const char* hidLabel = "Hidden";
        ImVec2 dlSize = ImGui::CalcTextSize(defLabel);
        ImVec2 vlSize = ImGui::CalcTextSize(visLabel);
        ImVec2 hlSize = ImGui::CalcTextSize(hidLabel);
        float labelY = winPos.y + ph - dlSize.y - 6;

        TextShadow(dl, ImVec2(cx1 - dlSize.x * 0.5f, labelY),
                   IM_COL32(180, 180, 180, 255), defLabel);
        TextShadow(dl, ImVec2(cx2 - vlSize.x * 0.5f, labelY), visCol, visLabel);
        TextShadow(dl, ImVec2(cx3 - hlSize.x * 0.5f, labelY), hidCol, hidLabel);
    } else {
        
        const float boxH = ph * 0.44f;
        const float boxW = boxH * 0.48f;
        const float boneScale = boxH / 160.0f;
        const float cx = winPos.x + pw * 0.5f;
        const float boxTop = winPos.y + ph * 0.18f;

        ImU32 entityCol = Col4(g::espBoxColor);

        DrawPlayerSilhouette(dl, cx, boxTop, boxW, boxH, boneScale,
                             entityCol, true, winPos.y + ph);

        
        if (g::espOffscreenArrows) {
            ImU32 arrowCol = Col4(g::espOffscreenColor);
            float ax = winPos.x + 16;
            float ay = winPos.y + ph * 0.5f;
            float sz = 10.0f;
            ImVec2 p1(ax, ay - sz);
            ImVec2 p2(ax + sz * 1.2f, ay);
            ImVec2 p3(ax, ay + sz);
            dl->AddTriangleFilled(p1, p2, p3, arrowCol);
            dl->AddTriangle(p1, p2, p3, IM_COL32(0, 0, 0, 200), 1.5f);
        }
    }

    ImGui::Dummy(avail);
    ImGui::End();
}
