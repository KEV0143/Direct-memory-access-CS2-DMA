#include "Features/ESP/Render/style.h"

void esp::Draw()
{
    if (!g::espEnabled && !g::radarEnabled && !g::radarSpectatorList && !g::espBombTime) return;

    const float screenW = static_cast<float>(g::screenWidth);
    const float screenH = static_cast<float>(g::screenHeight);
    if (screenW <= 0 || screenH <= 0) return;

    
    const int readIdx = s_readIdx.load(std::memory_order_acquire);
    const EntitySnapshot& snap = s_entityBuf[readIdx];

    const esp::PlayerData* players = snap.players;
    const esp::PlayerData* prevPlayers = snap.prevPlayers;
    view_matrix_t viewMatrix = {};
    const int localTeam = snap.localTeam;
    Vector3 localPos = snap.localPos;
    Vector3 prevLocalPos = snap.prevLocalPos;
    Vector3 viewAngles = snap.viewAngles;
    const bool localHasBomb = snap.localHasBomb;
    const Vector3 minimapMins = snap.minimapMins;
    const Vector3 minimapMaxs = snap.minimapMaxs;
    const bool hasMinimapBounds = snap.hasMinimapBounds;
    const bool localMaskResolved = snap.localMaskResolved;
    const WorldMarker* worldMarkers = snap.worldMarkers;
    const int worldMarkerCount = snap.worldMarkerCount;
    const BombState bombState = snap.bombState;
    const uint64_t captureTimeUs = snap.captureTimeUs;
    const uint64_t prevCaptureTimeUs = snap.prevCaptureTimeUs;
    const uint64_t nowUs = TickNowUs();

    memcpy(&viewMatrix, &snap.viewMatrix, sizeof(view_matrix_t));
    bool usingLiveLocalPos = false;
    auto isLikelyViewMatrix = [](const view_matrix_t& matrix) -> bool {
        float absSum = 0.0f;
        int nonZeroCount = 0;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                const float value = matrix[row][col];
                if (!std::isfinite(value))
                    return false;
                absSum += std::fabs(value);
                if (std::fabs(value) > 0.0001f)
                    ++nonZeroCount;
            }
        }
        return nonZeroCount >= 6 && absSum > 1.0f;
    };
    static view_matrix_t s_lastGoodDrawViewMatrix = {};
    static bool s_lastGoodDrawViewMatrixValid = false;
    if (isLikelyViewMatrix(viewMatrix)) {
        memcpy(&s_lastGoodDrawViewMatrix, &viewMatrix, sizeof(view_matrix_t));
        s_lastGoodDrawViewMatrixValid = true;
    } else if (s_lastGoodDrawViewMatrixValid) {
        memcpy(&viewMatrix, &s_lastGoodDrawViewMatrix, sizeof(view_matrix_t));
    }

    
    {
        std::lock_guard<std::mutex> lock(s_cameraMutex);
        const bool liveViewFresh =
            s_liveViewValid &&
            s_liveViewUpdatedUs > 0 &&
            nowUs >= s_liveViewUpdatedUs &&
            (nowUs - s_liveViewUpdatedUs) <= kLiveCameraFreshnessUs;
        if (liveViewFresh && isLikelyViewMatrix(s_liveViewMatrix)) {
            memcpy(&viewMatrix, &s_liveViewMatrix, sizeof(view_matrix_t));
            viewAngles = s_liveViewAngles;
            memcpy(&s_lastGoodDrawViewMatrix, &viewMatrix, sizeof(view_matrix_t));
            s_lastGoodDrawViewMatrixValid = true;
        } else if (!isLikelyViewMatrix(viewMatrix) && s_lastGoodDrawViewMatrixValid) {
            memcpy(&viewMatrix, &s_lastGoodDrawViewMatrix, sizeof(view_matrix_t));
        }

        const bool liveLocalPosFresh =
            s_liveLocalPosValid &&
            s_liveLocalPosUpdatedUs > 0 &&
            nowUs >= s_liveLocalPosUpdatedUs &&
            (nowUs - s_liveLocalPosUpdatedUs) <= kLiveCameraFreshnessUs;
        if (liveLocalPosFresh) {
            localPos = s_liveLocalPos;
            usingLiveLocalPos = true;
        }
    }

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList)
        return;

    const bool usableViewMatrix = isLikelyViewMatrix(viewMatrix);

    
    const uint64_t nowMs = nowUs / 1000u;
    const uint64_t nominalIntervalUs = 1000000u / DATA_WORKER_HZ;
    const uint64_t snapshotIntervalUs =
        (captureTimeUs > prevCaptureTimeUs && prevCaptureTimeUs > 0)
        ? (captureTimeUs - prevCaptureTimeUs)
        : nominalIntervalUs;
    
    const uint64_t renderDelayUs = std::min(snapshotIntervalUs, nominalIntervalUs + 500u);
    const uint64_t targetUs = (nowUs > renderDelayUs) ? (nowUs - renderDelayUs) : nowUs;

    float snapshotLerpAlpha = 1.0f;
    float extrapolationSec = 0.0f;
    if (captureTimeUs > prevCaptureTimeUs && prevCaptureTimeUs > 0) {
        if (targetUs <= prevCaptureTimeUs) {
            snapshotLerpAlpha = 0.0f;
        } else if (targetUs < captureTimeUs) {
            snapshotLerpAlpha = static_cast<float>(targetUs - prevCaptureTimeUs) /
                                static_cast<float>(captureTimeUs - prevCaptureTimeUs);
        } else {
            snapshotLerpAlpha = 1.0f;
            extrapolationSec = std::clamp(
                static_cast<float>(targetUs - captureTimeUs) / 1000000.0f,
                0.0f,
                0.055f);
        }
    }

    const float hermiteT = snapshotLerpAlpha * snapshotLerpAlpha * (3.0f - 2.0f * snapshotLerpAlpha);
    auto lerpVec3 = [&](const Vector3& a, const Vector3& b) -> Vector3 {
        return {
            a.x + (b.x - a.x) * hermiteT,
            a.y + (b.y - a.y) * hermiteT,
            a.z + (b.z - a.z) * hermiteT
        };
    };

    Vector3 renderLocalPos = localPos;
    if (!usingLiveLocalPos && prevCaptureTimeUs > 0 && captureTimeUs > prevCaptureTimeUs)
        renderLocalPos = lerpVec3(prevLocalPos, localPos);

    const float yawRad = viewAngles.y * (std::numbers::pi_v<float> / 180.0f);
    const float yawSin = sinf(yawRad);
    const float yawCos = cosf(yawRad);

    ImU32 boxCol = ColorToImU32(g::espBoxColor);
    ImU32 armorCol = ColorToImU32(g::espArmorColor);
    ImU32 nameCol = ColorToImU32(g::espNameColor);
    ImU32 distanceCol = ColorToImU32(g::espDistanceColor);
    ImU32 visibleCol = ColorToImU32(g::espVisibleColor);
    ImU32 hiddenCol = ColorToImU32(g::espHiddenColor);
    ImU32 skelCol = ColorToImU32(g::espSkeletonColor);
    ImU32 snapCol = ColorToImU32(g::espSnaplineColor);
    ImU32 offscreenCol = ColorToImU32(g::espOffscreenColor);
    ImU32 worldCol = ColorToImU32(g::espWorldColor);
    ImU32 bombCol = ColorToImU32(g::espBombColor);
    auto getPlayerBone = [&](const esp::PlayerData& player, int boneId) -> Vector3 {
        const int idx = esp::PlayerStoredBoneIndex(boneId);
        if (idx < 0)
            return {};
        return player.bones[idx];
    };
    auto hasUsableBone = [&](const esp::PlayerData& player, int boneId) -> bool {
        const Vector3 bone = getPlayerBone(player, boneId);
        return isFiniteVec(bone) &&
               !(bone.x == 0.0f && bone.y == 0.0f && bone.z == 0.0f);
    };
    auto isPlausibleToeSegment = [&](const Vector3& heel, const Vector3& toe) -> bool {
        if (!isFiniteVec(heel) || !isFiniteVec(toe))
            return false;
        const Vector3 delta = toe - heel;
        const float distance = delta.Length();
        return distance >= 1.0f &&
               distance <= 30.0f &&
               std::fabs(delta.z) <= 18.0f;
    };
    auto selectToeBoneId = [&](const esp::PlayerData& player, bool leftSide) -> int {
        const int primary = leftSide
            ? esp::LeftToeBoneForTeam(player.team)
            : esp::RightToeBoneForTeam(player.team);
        const int alternate = leftSide
            ? (primary == esp::FOOT_TOES_L_CT ? esp::FOOT_TOES_L_T : esp::FOOT_TOES_L_CT)
            : (primary == esp::FOOT_TOES_R_CT ? esp::FOOT_TOES_R_T : esp::FOOT_TOES_R_CT);
        if (hasUsableBone(player, primary))
            return primary;
        if (hasUsableBone(player, alternate))
            return alternate;
        return -1;
    };

    if (g::espEnabled && usableViewMatrix) {
        for (int i = 0; i < 64; i++) {
            const esp::PlayerData& p = players[i];
            if (!p.valid)
                continue;
            if (p.health <= 0)
                continue;
            if (snap.localPlayerIndex >= 0 && i == snap.localPlayerIndex)
                continue;
            if (snap.localPawn != 0 && p.pawn == snap.localPawn)
                continue;
            if ((localTeam == 2 || localTeam == 3) && p.team == localTeam)
                continue;

            const esp::PlayerData& prevP = prevPlayers[i];
            const bool canBlendSnapshots =
                prevP.valid &&
                prevP.pawn != 0 &&
                prevP.pawn == p.pawn &&
                captureTimeUs > prevCaptureTimeUs;

            if (!isValidWorldPos(p.position))
                continue;

            Vector3 predictedPos = canBlendSnapshots ? lerpVec3(prevP.position, p.position) : p.position;
            if (extrapolationSec > 0.0f && isFiniteVec(p.velocity)) {
                const float velocity2D = static_cast<float>(std::hypot(p.velocity.x, p.velocity.y));
                if (velocity2D > 1.0f) {
                    predictedPos.x += p.velocity.x * extrapolationSec;
                    predictedPos.y += p.velocity.y * extrapolationSec;
                    predictedPos.z += p.velocity.z * extrapolationSec;
                }
            }
            const Vector3 renderPlayerPos = predictedPos;
            if (!isValidWorldPos(renderPlayerPos))
                continue;
            const Vector3 positionOffset = renderPlayerPos - p.position;
            const Vector3 velocityOffset = (extrapolationSec > 0.0f && isFiniteVec(p.velocity))
                ? p.velocity * extrapolationSec : Vector3{0.0f, 0.0f, 0.0f};
            const Vector3 fallbackFeetPos = renderPlayerPos;
            Vector3 fallbackHeadPos = renderPlayerPos;
            fallbackHeadPos.z += 72.0f;

            
            Vector3 feetPos;
            Vector3 headPos;
            ScreenPos boneScreen[esp::kSkeletonScreenBoneCapacity] = {};
            bool boneScreenValid[esp::kSkeletonScreenBoneCapacity] = {};
            const int leftToeBoneId = selectToeBoneId(p, true);
            const int rightToeBoneId = selectToeBoneId(p, false);
            const Vector3 headBone = getPlayerBone(p, esp::HEAD);
            const Vector3 pelvisBone = getPlayerBone(p, esp::PELVIS);
            const Vector3 chestBone = getPlayerBone(p, esp::CHEST);
            const Vector3 leftHeelBone = getPlayerBone(p, esp::FOOT_HEEL_L);
            const Vector3 rightHeelBone = getPlayerBone(p, esp::FOOT_HEEL_R);
            const Vector3 leftToeBone = leftToeBoneId >= 0 ? getPlayerBone(p, leftToeBoneId) : Vector3{};
            const Vector3 rightToeBone = rightToeBoneId >= 0 ? getPlayerBone(p, rightToeBoneId) : Vector3{};
            const bool hasHeadBone = hasUsableBone(p, esp::HEAD);
            const bool hasPelvisBone = hasUsableBone(p, esp::PELVIS);
            const bool hasChestBone = hasUsableBone(p, esp::CHEST);
            const bool hasLeftHeelBone = hasUsableBone(p, esp::FOOT_HEEL_L);
            const bool hasRightHeelBone = hasUsableBone(p, esp::FOOT_HEEL_R);
            const bool hasLeftToeBone =
                leftToeBoneId >= 0 &&
                hasLeftHeelBone &&
                isPlausibleToeSegment(leftHeelBone, leftToeBone);
            const bool hasRightToeBone =
                rightToeBoneId >= 0 &&
                hasRightHeelBone &&
                isPlausibleToeSegment(rightHeelBone, rightToeBone);
            const bool hasLeftFootBone = hasLeftHeelBone;
            const bool hasRightFootBone = hasRightHeelBone;
            const bool hasCoreBoneChain =
                hasHeadBone &&
                hasPelvisBone &&
                (hasChestBone || hasUsableBone(p, esp::SPINE2)) &&
                (hasLeftFootBone || hasRightFootBone);
            int validStoredBoneCount = 0;
            for (int bIdx = 0; bIdx < esp::kPlayerStoredBoneCount; ++bIdx) {
                if (hasUsableBone(p, esp::kPlayerStoredBoneIds[bIdx]))
                    ++validStoredBoneCount;
            }
            float skeletonHeight = 0.0f;
            if (hasHeadBone && (hasLeftFootBone || hasRightFootBone)) {
                float lowestFootZ = FLT_MAX;
                if (hasLeftHeelBone)
                    lowestFootZ = std::min(lowestFootZ, leftHeelBone.z);
                if (hasRightHeelBone)
                    lowestFootZ = std::min(lowestFootZ, rightHeelBone.z);
                skeletonHeight = headBone.z - lowestFootZ;
            }
            const float pelvisAnchor2D =
                hasPelvisBone
                ? static_cast<float>(std::hypot(
                    pelvisBone.x - renderPlayerPos.x,
                    pelvisBone.y - renderPlayerPos.y))
                : FLT_MAX;
            float footAnchor2D = FLT_MAX;
            if (hasLeftFootBone) {
                footAnchor2D = std::min(
                    footAnchor2D,
                    static_cast<float>(std::hypot(
                        leftHeelBone.x - renderPlayerPos.x,
                        leftHeelBone.y - renderPlayerPos.y)));
            }
            if (hasRightFootBone) {
                footAnchor2D = std::min(
                    footAnchor2D,
                    static_cast<float>(std::hypot(
                        rightHeelBone.x - renderPlayerPos.x,
                        rightHeelBone.y - renderPlayerPos.y)));
            }
            const bool plausibleBoneAnchor =
                pelvisAnchor2D <= 96.0f ||
                footAnchor2D <= 96.0f;
            const bool plausibleBoneHeight =
                skeletonHeight >= 20.0f &&
                skeletonHeight <= 112.0f;
            const bool renderHasReliableBones =
                p.hasBones &&
                hasCoreBoneChain &&
                validStoredBoneCount >= 6 &&
                plausibleBoneAnchor &&
                plausibleBoneHeight;
            int validBoneSegmentCount = 0;

            if (renderHasReliableBones) {
                headPos = headBone;
                if (canBlendSnapshots && prevP.hasBones)
                    headPos = lerpVec3(getPlayerBone(prevP, esp::HEAD), headBone);
                else
                    headPos = headPos + positionOffset;
                if (extrapolationSec > 0.0f)
                    headPos = headPos + velocityOffset;
                headPos.z += 8.0f;

                Vector3 leftFoot = leftHeelBone;
                Vector3 rightFoot = rightHeelBone;
                if (canBlendSnapshots && prevP.hasBones) {
                    const Vector3 prevLeftFoot = getPlayerBone(prevP, esp::FOOT_HEEL_L);
                    const Vector3 prevRightFoot = getPlayerBone(prevP, esp::FOOT_HEEL_R);
                    if (isFiniteVec(prevLeftFoot) &&
                        !(prevLeftFoot.x == 0.0f && prevLeftFoot.y == 0.0f && prevLeftFoot.z == 0.0f))
                        leftFoot = lerpVec3(prevLeftFoot, leftFoot);
                    if (isFiniteVec(prevRightFoot) &&
                        !(prevRightFoot.x == 0.0f && prevRightFoot.y == 0.0f && prevRightFoot.z == 0.0f))
                        rightFoot = lerpVec3(prevRightFoot, rightFoot);
                } else {
                    leftFoot = leftFoot + positionOffset;
                    rightFoot = rightFoot + positionOffset;
                }
                if (extrapolationSec > 0.0f) {
                    leftFoot = leftFoot + velocityOffset;
                    rightFoot = rightFoot + velocityOffset;
                }

                const bool hasLeft = !(leftFoot.x == 0.0f && leftFoot.y == 0.0f && leftFoot.z == 0.0f);
                const bool hasRight = !(rightFoot.x == 0.0f && rightFoot.y == 0.0f && rightFoot.z == 0.0f);
                if (hasLeft && hasRight)
                    feetPos = (leftFoot.z < rightFoot.z) ? leftFoot : rightFoot;
                else if (hasLeft)
                    feetPos = leftFoot;
                else if (hasRight)
                    feetPos = rightFoot;
                else
                    feetPos = renderPlayerPos;
                feetPos.z -= 4.0f;

                for (int bIdx = 0; bIdx < esp::kPlayerStoredBoneCount; ++bIdx) {
                    const int b = esp::kPlayerStoredBoneIds[bIdx];
                    Vector3 bonePos = getPlayerBone(p, b);
                    if (bonePos.x == 0.0f && bonePos.y == 0.0f && bonePos.z == 0.0f) {
                        boneScreenValid[b] = false;
                        continue;
                    }
                    if (canBlendSnapshots && prevP.hasBones) {
                        const Vector3 prevBone = getPlayerBone(prevP, b);
                        if (!(prevBone.x == 0.0f && prevBone.y == 0.0f && prevBone.z == 0.0f))
                            bonePos = lerpVec3(prevBone, bonePos);
                    } else {
                        bonePos = bonePos + positionOffset;
                    }
                    if (extrapolationSec > 0.0f)
                        bonePos = bonePos + velocityOffset;
                    boneScreen[b] = WorldToScreen(bonePos, viewMatrix, screenW, screenH);
                    boneScreenValid[b] = boneScreen[b].onScreen;
                }
                for (int pairIdx = 0; pairIdx < skeletonPairCount; ++pairIdx) {
                    const BonePair& pair = skeletonPairs[pairIdx];
                    if (boneScreenValid[pair.from] && boneScreenValid[pair.to])
                        ++validBoneSegmentCount;
                }
                if (boneScreenValid[esp::FOOT_HEEL_L] && hasLeftToeBone && boneScreenValid[leftToeBoneId])
                    ++validBoneSegmentCount;
                if (boneScreenValid[esp::FOOT_HEEL_R] && hasRightToeBone && boneScreenValid[rightToeBoneId])
                    ++validBoneSegmentCount;
            } else {
                feetPos = renderPlayerPos;
                headPos = feetPos;
                headPos.z += 72.0f;
            }

            if (!isFiniteVec(feetPos) || !isFiniteVec(headPos))
                continue;

            ScreenPos screenFeet = WorldToScreen(feetPos, viewMatrix, screenW, screenH);
            ScreenPos screenHead = WorldToScreen(headPos, viewMatrix, screenW, screenH);
            const ScreenPos fallbackScreenFeet = WorldToScreen(fallbackFeetPos, viewMatrix, screenW, screenH);
            const ScreenPos fallbackScreenHead = WorldToScreen(fallbackHeadPos, viewMatrix, screenW, screenH);
            bool usingFallbackBox = false;
            bool onScreen = screenFeet.onScreen && screenHead.onScreen;
            const bool fallbackOnScreen = fallbackScreenFeet.onScreen && fallbackScreenHead.onScreen;
            if ((!onScreen || screenFeet.y <= screenHead.y + 4.0f) && fallbackOnScreen) {
                screenFeet = fallbackScreenFeet;
                screenHead = fallbackScreenHead;
                feetPos = fallbackFeetPos;
                headPos = fallbackHeadPos;
                onScreen = true;
                usingFallbackBox = true;
            }
            if (!onScreen) {
#include "../Render/player_offscreen_arrows.inl"
                continue;
            }

            float boxHeight = screenFeet.y - screenHead.y;
            if (boxHeight < 4.0f && fallbackOnScreen) {
                screenFeet = fallbackScreenFeet;
                screenHead = fallbackScreenHead;
                feetPos = fallbackFeetPos;
                headPos = fallbackHeadPos;
                boxHeight = screenFeet.y - screenHead.y;
                usingFallbackBox = true;
            }
            if (boxHeight < 4.0f)
                continue;
            float boxWidth = boxHeight * 0.5f;
            float boxLeft = screenHead.x - boxWidth * 0.5f;
            float boxTop = screenHead.y;
            const float boxCenterX = boxLeft + boxWidth * 0.5f;
            const float sideBarWidth = 3.0f;
            const float sideBarGap = 4.0f;
            const float primaryLeftBarX = boxLeft - sideBarWidth - sideBarGap;
            const bool canRenderRealSkeleton =
                renderHasReliableBones &&
                validBoneSegmentCount >= 5;

            bool isVisibleNow = p.visible;
            if (!isVisibleNow && !localMaskResolved && p.spottedMask != 0ULL) {
                const bool onScreen169 =
                    boxLeft < (screenW - 1.0f) &&
                    (boxLeft + boxWidth) > 1.0f &&
                    boxTop < (screenH - 1.0f) &&
                    (boxTop + boxHeight) > 1.0f;
                if (onScreen169)
                    isVisibleNow = true;
            }
            const ImU32 entityCol = g::espVisibilityColoring ? (isVisibleNow ? visibleCol : hiddenCol) : boxCol;

#include "../Render/player_box.inl"
#include "../Render/player_health_armor.inl"
#include "../Render/player_name.inl"

            float bottomTextY = boxTop + boxHeight + 2.0f;
            auto drawBottomLabel = [&](const char* text, ImU32 color, bool requireVisible, bool strictBounds, ImFont* font, float fontSize) {
                if (!text || text[0] == '\0')
                    return;
                if (requireVisible && !isVisibleNow)
                    return;
                if (!font)
                    font = ImGui::GetFont();
                if (!font)
                    return;
                if (fontSize <= 0.0f)
                    fontSize = ImGui::GetFontSize();
                ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text, nullptr);
                float textX = boxCenterX - textSize.x * 0.5f;
                const float textY = bottomTextY;
                if (strictBounds) {
                    if (textX < 2.0f || textX + textSize.x > screenW - 2.0f || textY + textSize.y > screenH - 2.0f)
                        return;
                } else {
                    if (textX < 2.0f) textX = 2.0f;
                    if (textX + textSize.x > screenW - 2.0f) textX = screenW - textSize.x - 2.0f;
                    if (textY + textSize.y > screenH - 2.0f)
                        return;
                }
                drawList->AddText(font, fontSize, ImVec2(textX + 1.0f, textY + 1.0f), IM_COL32(0, 0, 0, 210), text);
                drawList->AddText(font, fontSize, ImVec2(textX, textY), color, text);
                bottomTextY += textSize.y + 1.0f;
            };

#include "../Render/player_weapon_and_bomb.inl"
#include "../Render/player_weapon_ammo.inl"
#include "../Render/player_flags.inl"
#include "../Render/player_skeleton.inl"
#include "../Render/player_snaplines.inl"
#include "../Render/player_sound.inl"
        }
    }

    if (usableViewMatrix) {
#include "../Render/world_esp.inl"

#include "../Render/bomb_esp.inl"
    }

#include "Features/Radar/Render/radar_overlay.inl"
#include "../Render/bomb_timer_overlay.inl"
#include "../Render/spectator_list_overlay.inl"
}
