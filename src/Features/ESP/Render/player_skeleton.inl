if (g::espSkeleton) {
    const ImU32 skeletonRenderCol = g::espVisibilityColoring ? entityCol : skelCol;
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

        if (g::espSkeletonDots) {
            const int jointBones[] = { 0, 2, 5, 8, 9, 10, 13, 14, 15, 22, 23, 24, 25, 26, 27 };
            for (int jIdx = 0; jIdx < 15; ++jIdx) {
                const int b = jointBones[jIdx];
                if (!boneScreenValid[b]) continue;
                const ImVec2 jp(boneScreen[b].x, boneScreen[b].y);
                drawList->AddCircleFilled(jp, 2.2f, IM_COL32(0, 0, 0, 180), 6);
                drawList->AddCircleFilled(jp, 1.4f, skeletonRenderCol, 6);
            }
        }
    }
}
