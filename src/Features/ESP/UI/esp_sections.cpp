#include "Features/ESP/UI/esp_sections.h"

#include "app/Core/globals.h"

#include <imgui.h>

namespace
{
    enum class ItemEspGroup {
        Pistols,
        SMGs,
        Rifles,
        Snipers,
        Heavy,
        Gear
    };

    struct ItemEspEntry {
        uint16_t id;
        const char* label;
        ItemEspGroup group;
    };

    constexpr ItemEspEntry kItemEspEntries[] = {
        { 1, "Deagle", ItemEspGroup::Pistols },
        { 2, "Dual Berettas", ItemEspGroup::Pistols },
        { 3, "Five-SeveN", ItemEspGroup::Pistols },
        { 4, "Glock", ItemEspGroup::Pistols },
        { 30, "Tec-9", ItemEspGroup::Pistols },
        { 32, "P2000", ItemEspGroup::Pistols },
        { 36, "P250", ItemEspGroup::Pistols },
        { 61, "USP-S", ItemEspGroup::Pistols },
        { 63, "CZ75", ItemEspGroup::Pistols },
        { 64, "R8", ItemEspGroup::Pistols },
        { 17, "MAC-10", ItemEspGroup::SMGs },
        { 19, "P90", ItemEspGroup::SMGs },
        { 23, "MP5", ItemEspGroup::SMGs },
        { 24, "UMP-45", ItemEspGroup::SMGs },
        { 26, "PP-Bizon", ItemEspGroup::SMGs },
        { 33, "MP7", ItemEspGroup::SMGs },
        { 34, "MP9", ItemEspGroup::SMGs },
        { 7, "AK-47", ItemEspGroup::Rifles },
        { 8, "AUG", ItemEspGroup::Rifles },
        { 10, "FAMAS", ItemEspGroup::Rifles },
        { 13, "Galil", ItemEspGroup::Rifles },
        { 16, "M4A4", ItemEspGroup::Rifles },
        { 39, "SG553", ItemEspGroup::Rifles },
        { 60, "M4A1-S", ItemEspGroup::Rifles },
        { 9, "AWP", ItemEspGroup::Snipers },
        { 11, "G3SG1", ItemEspGroup::Snipers },
        { 38, "SCAR-20", ItemEspGroup::Snipers },
        { 40, "SSG08", ItemEspGroup::Snipers },
        { 14, "M249", ItemEspGroup::Heavy },
        { 25, "XM1014", ItemEspGroup::Heavy },
        { 27, "MAG-7", ItemEspGroup::Heavy },
        { 28, "Negev", ItemEspGroup::Heavy },
        { 29, "Sawed-Off", ItemEspGroup::Heavy },
        { 35, "Nova", ItemEspGroup::Heavy },
        { 31, "Zeus", ItemEspGroup::Gear },
        { 57, "Healthshot", ItemEspGroup::Gear },
    };
    constexpr ImGuiColorEditFlags kPickerFlags =
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_AlphaPreviewHalf;

    constexpr ImGuiColorEditFlags kInlineColorFlags =
        kPickerFlags | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;

    constexpr float kColorOffsetX = 130.0f;
    constexpr float kSliderWidth = 160.0f;

    template <typename ExtraFn>
    void DrawFeature(const char* id, const char* label, bool* enabled, float* color, ExtraFn&& extraFn)
    {
        ImGui::PushID(id);
        ImGui::Checkbox(label, enabled);

        const bool disabled = !*enabled;
        ImGui::SameLine();
        ImGui::BeginDisabled(disabled);

        if (ImGui::SmallButton("Settings"))
            ImGui::OpenPopup("##cfg");

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
        ImGui::SetNextWindowSizeConstraints(ImVec2(520.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::BeginPopup("##cfg")) {
            ImGui::TextUnformatted(label);
            ImGui::Separator();
            ImGui::Spacing();

            if (color) {
                ImGui::TextDisabled("Color");
                ImGui::SameLine(kColorOffsetX);
                ImGui::SetNextItemWidth(26.0f);
                ImGui::ColorEdit4("##clr", color, kInlineColorFlags);
            }

            extraFn();

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        ImGui::EndDisabled();
        ImGui::PopID();
    }

    void ColorRow(const char* id, const char* label, float* color)
    {
        ImGui::PushID(id);
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(kColorOffsetX);
        ImGui::SetNextItemWidth(26.0f);
        ImGui::ColorEdit4("##c", color, kInlineColorFlags);
        ImGui::PopID();
    }

    void ToggleColorRow(const char* id, const char* label, bool* toggle, float* color)
    {
        ImGui::PushID(id);
        ImGui::Checkbox(label, toggle);
        ImGui::SameLine(kColorOffsetX);
        ImGui::SetNextItemWidth(26.0f);
        ImGui::ColorEdit4("##c", color, kInlineColorFlags);
        ImGui::PopID();
    }

    void SizeRow(const char* id, const char* label, float* size, float minValue, float maxValue)
    {
        ImGui::PushID(id);
        ImGui::TextDisabled("%s", label);
        ImGui::SameLine(kColorOffsetX);
        ImGui::SetNextItemWidth(kSliderWidth);
        ImGui::SliderFloat("##s", size, minValue, maxValue, *size == 0.0f ? "Default" : "%.0f");
        ImGui::PopID();
    }

    const char* ItemEspGroupLabel(ItemEspGroup group)
    {
        switch (group) {
        case ItemEspGroup::Pistols: return "Pistols";
        case ItemEspGroup::SMGs: return "SMGs";
        case ItemEspGroup::Rifles: return "Rifles";
        case ItemEspGroup::Snipers: return "Snipers";
        case ItemEspGroup::Heavy: return "Heavy";
        case ItemEspGroup::Gear: return "Gear";
        default: break;
        }
        return "Items";
    }

    void SetItemEspEnabled(uint16_t id, bool enabled)
    {
        if (id == 0 || id >= 1200)
            return;
        g::espItemEnabledMask.set(id, enabled);
    }

    bool IsItemEspEnabled(uint16_t id)
    {
        if (id == 0 || id >= 1200)
            return false;
        return g::espItemEnabledMask.test(id);
    }

    void SetAllKnownItemsEnabled(bool enabled)
    {
        for (const ItemEspEntry& entry : kItemEspEntries)
            SetItemEspEnabled(entry.id, enabled);
    }

    void RenderItemGroupBlock(ItemEspGroup group)
    {
        ImGui::TextDisabled("%s", ItemEspGroupLabel(group));
        ImGui::Separator();
        ImGui::Spacing();

        for (const ItemEspEntry& entry : kItemEspEntries) {
            if (entry.group != group)
                continue;

            bool enabled = IsItemEspEnabled(entry.id);
            if (ImGui::Checkbox(entry.label, &enabled))
                SetItemEspEnabled(entry.id, enabled);
        }

        ImGui::Spacing();
    }
}

void ui::tabs::esp_sections::RenderGeneralSection()
{
    DrawFeature("box", "Corner Box", &g::espBox, g::espBoxColor, [] {});
    DrawFeature("health", "Health Bar", &g::espHealth, g::espHealthColor, [] {
        ImGui::Checkbox("Show Value", &g::espHealthText);
    });
    DrawFeature("armor", "Armor Bar", &g::espArmor, g::espArmorColor, [] {
        ImGui::Checkbox("Show Value", &g::espArmorText);
    });

    DrawFeature("vis", "Visibility Colors", &g::espVisibilityColoring, g::espVisibleColor, [] {
        ColorRow("occ", "Occluded", g::espHiddenColor);
    });
}

void ui::tabs::esp_sections::RenderWeaponSection()
{
    DrawFeature("weapon", "Weapon Label", &g::espWeapon, nullptr, [] {
        ToggleColorRow("txt", "Label Text", &g::espWeaponText, g::espWeaponTextColor);
        SizeRow("txtsz", "Text Size", &g::espWeaponTextSize, 0.0f, 24.0f);
        ImGui::Separator();
        ToggleColorRow("icon", "Icon Weapon", &g::espWeaponIcon, g::espWeaponIconColor);
        ImGui::Checkbox("No Knife", &g::espWeaponIconNoKnife);
        SizeRow("iconsz", "Icon Size", &g::espWeaponIconSize, 10.0f, 30.0f);
        ImGui::Separator();
        ToggleColorRow("ammo", "Weapon Ammo", &g::espWeaponAmmo, g::espWeaponAmmoColor);
        SizeRow("ammosz", "Ammo Size", &g::espWeaponAmmoSize, 0.0f, 24.0f);
    });
}

void ui::tabs::esp_sections::RenderPlayerVisualsSection()
{
    DrawFeature("skeleton", "Skeleton", &g::espSkeleton, g::espSkeletonColor, [] {
        ImGui::Checkbox("Show Dots", &g::espSkeletonDots);
    });

    DrawFeature("snap", "Snap Lines", &g::espSnaplines, g::espSnaplineColor, [] {
        ImGui::Checkbox("Snap From Top", &g::espSnaplineFromTop);
    });

    DrawFeature("awareness", "Screen Arrows", &g::espOffscreenArrows, g::espOffscreenColor, [] {
        ImGui::Checkbox("Sound ESP", &g::espSound);
    });

    if (!g::espOffscreenArrows)
        g::espSound = false;
}

void ui::tabs::esp_sections::RenderFlagsSection()
{
    DrawFeature("flags", "Player Flags", &g::espFlags, nullptr, [] {
        const auto flagRow = [](const char* id, const char* label, bool* toggle, float* color, float* size) {
            ImGui::PushID(id);
            ImGui::Checkbox(label, toggle);
            ImGui::SameLine(kColorOffsetX);
            ImGui::SetNextItemWidth(26.0f);
            ImGui::ColorEdit4("##c", color, kInlineColorFlags);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(kSliderWidth);
            ImGui::SliderFloat("##s", size, 0.0f, 24.0f, *size == 0.0f ? "Default" : "%.0f");
            ImGui::PopID();
        };

        flagRow("name", "Name", &g::espName, g::espNameColor, &g::espNameFontSize);
        flagRow("distance", "Distance", &g::espDistance, g::espDistanceColor, &g::espDistanceSize);
        ImGui::Separator();
        flagRow("blind", "Blind", &g::espFlagBlind, g::espFlagBlindColor, &g::espFlagBlindSize);
        flagRow("scoped", "Scoped", &g::espFlagScoped, g::espFlagScopedColor, &g::espFlagScopedSize);
        flagRow("defusing", "Defusing", &g::espFlagDefusing, g::espFlagDefusingColor, &g::espFlagDefusingSize);
        flagRow("kit", "Kit", &g::espFlagKit, g::espFlagKitColor, &g::espFlagKitSize);
        flagRow("money", "Money", &g::espFlagMoney, g::espFlagMoneyColor, &g::espFlagMoneySize);
    });
}

void ui::tabs::esp_sections::RenderItemSection()
{
    DrawFeature("item", "Item ESP", &g::espItem, g::espWorldColor, [] {
        if (ImGui::Button("Enable All"))
            SetAllKnownItemsEnabled(true);
        ImGui::SameLine();
        if (ImGui::Button("Disable All"))
            SetAllKnownItemsEnabled(false);

        ImGui::Separator();

        ImGui::BeginChild("##itemfilter", ImVec2(0.0f, 380.0f), ImGuiChildFlags_Borders);
        if (ImGui::BeginTable("##itemgrid", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX)) {
            ImGui::TableNextColumn();
            RenderItemGroupBlock(ItemEspGroup::Pistols);
            ImGui::Spacing();
            RenderItemGroupBlock(ItemEspGroup::SMGs);

            ImGui::TableNextColumn();
            RenderItemGroupBlock(ItemEspGroup::Rifles);
            ImGui::Spacing();
            RenderItemGroupBlock(ItemEspGroup::Snipers);

            ImGui::TableNextColumn();
            RenderItemGroupBlock(ItemEspGroup::Heavy);
            ImGui::Spacing();
            RenderItemGroupBlock(ItemEspGroup::Gear);

            ImGui::EndTable();
        }
        ImGui::EndChild();
    });
}

void ui::tabs::esp_sections::RenderWorldSection()
{
    DrawFeature("world", "World ESP", &g::espWorld, g::espWorldColor, [] {
        ImGui::Checkbox("Smoke Timer", &g::espWorldSmokeTimer);
        ImGui::Checkbox("Molotov Timer", &g::espWorldInfernoTimer);
        ImGui::Checkbox("Decoy Timer", &g::espWorldDecoyTimer);
    });

    DrawFeature("bomb", "Bomb ESP", &g::espBombInfo, g::espBombColor, [] {
        ImGui::Checkbox("Show Text", &g::espBombText);
        SizeRow("bmbtxtsz", "Text Size", &g::espBombTextSize, 0.0f, 24.0f);
    });
}
