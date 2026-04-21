if (g::espSkeleton) {
    const ImU32 skeletonRenderCol = g::espVisibilityColoring ? entityCol : skelCol;
    const int leftToeBoneId = selectToeBoneId(p, true);
    const int rightToeBoneId = selectToeBoneId(p, false);
    if (canRenderRealSkeleton) {
        for (int b = 0; b < skeletonPairCount; b++) {
            const int from = skeletonPairs[b].from;
            const int to = skeletonPairs[b].to;
            if (!boneScreenValid[from] || !boneScreenValid[to]) continue;
            const ImVec2 p1(boneScreen[from].x, boneScreen[from].y);
            const ImVec2 p2(boneScreen[to].x, boneScreen[to].y);
            drawList->AddLine(p1, p2, IM_COL32(0, 0, 0, 180), 3.0f);
            drawList->AddLine(p1, p2, skeletonRenderCol, 1.6f);
        }
        if (hasLeftToeBone && boneScreenValid[esp::FOOT_HEEL_L] && boneScreenValid[leftToeBoneId]) {
            const ImVec2 p1(boneScreen[esp::FOOT_HEEL_L].x, boneScreen[esp::FOOT_HEEL_L].y);
            const ImVec2 p2(boneScreen[leftToeBoneId].x, boneScreen[leftToeBoneId].y);
            drawList->AddLine(p1, p2, IM_COL32(0, 0, 0, 180), 3.0f);
            drawList->AddLine(p1, p2, skeletonRenderCol, 1.6f);
        }
        if (hasRightToeBone && boneScreenValid[esp::FOOT_HEEL_R] && boneScreenValid[rightToeBoneId]) {
            const ImVec2 p1(boneScreen[esp::FOOT_HEEL_R].x, boneScreen[esp::FOOT_HEEL_R].y);
            const ImVec2 p2(boneScreen[rightToeBoneId].x, boneScreen[rightToeBoneId].y);
            drawList->AddLine(p1, p2, IM_COL32(0, 0, 0, 180), 3.0f);
            drawList->AddLine(p1, p2, skeletonRenderCol, 1.6f);
        }

        if (g::espSkeletonDots) {
            const int jointBones[] = {
                esp::PELVIS,
                esp::SPINE1,
                esp::SPINE2,
                esp::CHEST,
                esp::NECK,
                esp::SHOULDER_L,
                esp::ELBOW_L,
                esp::HAND_L,
                esp::SHOULDER_R,
                esp::ELBOW_R,
                esp::HAND_R,
                esp::HIP_L,
                esp::KNEE_L,
                esp::FOOT_HEEL_L,
                esp::HIP_R,
                esp::KNEE_R,
                esp::FOOT_HEEL_R,
                esp::HEAD
            };
            for (int jIdx = 0; jIdx < static_cast<int>(sizeof(jointBones) / sizeof(jointBones[0])); ++jIdx) {
                const int b = jointBones[jIdx];
                if (!boneScreenValid[b]) continue;
                const ImVec2 jp(boneScreen[b].x, boneScreen[b].y);
                drawList->AddCircleFilled(jp, 2.2f, IM_COL32(0, 0, 0, 180), 6);
                drawList->AddCircleFilled(jp, 1.4f, skeletonRenderCol, 6);
            }
            if (hasLeftToeBone && boneScreenValid[leftToeBoneId]) {
                const ImVec2 jp(boneScreen[leftToeBoneId].x, boneScreen[leftToeBoneId].y);
                drawList->AddCircleFilled(jp, 2.2f, IM_COL32(0, 0, 0, 180), 6);
                drawList->AddCircleFilled(jp, 1.4f, skeletonRenderCol, 6);
            }
            if (hasRightToeBone && boneScreenValid[rightToeBoneId]) {
                const ImVec2 jp(boneScreen[rightToeBoneId].x, boneScreen[rightToeBoneId].y);
                drawList->AddCircleFilled(jp, 2.2f, IM_COL32(0, 0, 0, 180), 6);
                drawList->AddCircleFilled(jp, 1.4f, skeletonRenderCol, 6);
            }
        }
    }
}
