        const bool bombHasDroppedEvidence = IsDroppedBombResolveKind(bombResolve.kind);

        const bool bombDefinitelyCarried =
            bombResolve.kind == BombResolveKind::Carried;
        
        
        
        
        
        const bool bombStrongCarryTransition =
            bombResolve.kind == BombResolveKind::Carried &&
            (bombResolve.sourceFlags & BombResolveSourceCarrySignal) != 0;
        const bool bombPlantedTransition =
            bombResolve.kind == BombResolveKind::Planted;
        if (bombStrongCarryTransition || bombPlantedTransition) {
            s_lastDroppedBombPos = { NAN, NAN, NAN };
            s_lastDroppedBombPosUs = 0;
            if (bombStrongCarryTransition) {
                s_lastVisibleBombPos = { NAN, NAN, NAN };
                s_lastVisibleBombBoundsMins = {};
                s_lastVisibleBombBoundsMaxs = {};
                s_lastVisibleBombBoundsValid = false;
                s_lastVisibleBombPosUs = 0;
            }
        } else if (bombHasDroppedEvidence && isValidWorldPos(bombResolve.position)) {
            droppedBombPos = bombResolve.position;
            droppedBombPosValid = true;
            droppedBombBoundsMins = bombResolve.boundsMins;
            droppedBombBoundsMaxs = bombResolve.boundsMaxs;
            droppedBombBoundsValid = bombResolve.boundsValid;
            s_lastDroppedBombPos = droppedBombPos;
            s_lastDroppedBombPosUs = nowUs;
        } else if (s_lastDroppedBombPosUs > 0 &&
                   nowUs >= s_lastDroppedBombPosUs &&
                   (nowUs - s_lastDroppedBombPosUs) <= esp::intervals::kBombStickyDroppedUs &&
                   isValidWorldPos(s_lastDroppedBombPos)) {
            droppedBombPos = s_lastDroppedBombPos;
            droppedBombPosValid = true;
            droppedBombBoundsMins = {};
            droppedBombBoundsMaxs = {};
            droppedBombBoundsValid = false;
        }

        s_bombState.planted = (bombResolve.kind == BombResolveKind::Planted);
        s_bombState.ticking = bombTicking != 0;
        s_bombState.beingDefused = bombBeingDefused != 0;
        s_bombState.dropped = bombHasDroppedEvidence;
        s_bombState.sourceFlags = bombResolve.sourceFlags;
        s_bombState.confidence = bombResolve.confidence;

        Vector3 targetBombPos = { NAN, NAN, NAN };
        Vector3 targetBombBoundsMins = {};
        Vector3 targetBombBoundsMaxs = {};
        bool targetBombBoundsValid = false;
        bool reusedConfirmedBombState = false;

        if (bombResolve.kind == BombResolveKind::Planted && isValidWorldPos(bombResolve.position)) {
            targetBombPos = bombResolve.position;
            targetBombBoundsMins = bombResolve.boundsMins;
            targetBombBoundsMaxs = bombResolve.boundsMaxs;
            targetBombBoundsValid =
                bombResolve.boundsValid &&
                isValidBombBounds(targetBombBoundsMins, targetBombBoundsMaxs);
        } else if (bombHasDroppedEvidence && isValidWorldPos(droppedBombPos)) {
            targetBombPos = droppedBombPos;
            targetBombBoundsMins = droppedBombBoundsMins;
            targetBombBoundsMaxs = droppedBombBoundsMaxs;
            targetBombBoundsValid =
                droppedBombBoundsValid &&
                isValidBombBounds(targetBombBoundsMins, targetBombBoundsMaxs);
        } else if (!sceneSettling &&
                   s_lastVisibleBombPosUs > 0 &&
                   nowUs >= s_lastVisibleBombPosUs &&
                   (nowUs - s_lastVisibleBombPosUs) <= esp::intervals::kBombStickyVisibleUs &&
                   isValidWorldPos(s_lastVisibleBombPos) &&
                   (s_bombState.planted || s_bombState.dropped)) {
            targetBombPos = s_lastVisibleBombPos;
            targetBombBoundsMins = s_lastVisibleBombBoundsMins;
            targetBombBoundsMaxs = s_lastVisibleBombBoundsMaxs;
            targetBombBoundsValid = s_lastVisibleBombBoundsValid;
        } else if (!sceneSettling &&
                   !bombDefinitelyCarried &&
                   s_lastConfirmedBombStateUs > 0 &&
                   nowUs >= s_lastConfirmedBombStateUs &&
                   (nowUs - s_lastConfirmedBombStateUs) <= 300000 &&
                   isValidWorldPos(s_lastConfirmedBombState.position) &&
                   (s_lastConfirmedBombState.planted || s_lastConfirmedBombState.dropped)) {
            if (!s_bombState.planted && !s_bombState.dropped) {
                s_bombState.planted = s_lastConfirmedBombState.planted;
                s_bombState.ticking = s_lastConfirmedBombState.ticking;
                s_bombState.beingDefused = s_lastConfirmedBombState.beingDefused;
                s_bombState.dropped = s_lastConfirmedBombState.dropped;
                s_bombState.sourceFlags = s_lastConfirmedBombState.sourceFlags;
                s_bombState.confidence = s_lastConfirmedBombState.confidence;
                s_bombState.blowTime = s_lastConfirmedBombState.blowTime;
                s_bombState.defuseEndTime = s_lastConfirmedBombState.defuseEndTime;
                s_bombState.currentGameTime = s_lastConfirmedBombState.currentGameTime;
                reusedConfirmedBombState = true;
            }
            targetBombPos = s_lastConfirmedBombState.position;
            targetBombBoundsMins = s_lastConfirmedBombState.boundsMins;
            targetBombBoundsMaxs = s_lastConfirmedBombState.boundsMaxs;
            targetBombBoundsValid = s_lastConfirmedBombState.boundsValid;
        }

        if (!targetBombBoundsValid && isValidWorldPos(targetBombPos)) {
            if (s_bombState.planted) {
                targetBombBoundsMins = Vector3(-9.0f, -9.0f, -1.0f);
                targetBombBoundsMaxs = Vector3(9.0f, 9.0f, 20.0f);
            } else {
                targetBombBoundsMins = Vector3(-10.0f, -10.0f, -3.0f);
                targetBombBoundsMaxs = Vector3(10.0f, 10.0f, 10.0f);
            }
            targetBombBoundsValid = true;
        }

        if (isValidWorldPos(targetBombPos)) {
            s_lastVisibleBombPos = targetBombPos;
            s_lastVisibleBombBoundsMins = targetBombBoundsMins;
            s_lastVisibleBombBoundsMaxs = targetBombBoundsMaxs;
            s_lastVisibleBombBoundsValid = targetBombBoundsValid;
            s_lastVisibleBombPosUs = nowUs;
            s_bombState.position = targetBombPos;
            s_bombState.rawPosition = targetBombPos;
        } else {
            s_bombState.position = { NAN, NAN, NAN };
            s_bombState.rawPosition = { NAN, NAN, NAN };
        }

        s_bombState.boundsMins = targetBombBoundsMins;
        s_bombState.boundsMaxs = targetBombBoundsMaxs;
        s_bombState.boundsValid = targetBombBoundsValid;

        const bool gameTimeValid =
            std::isfinite(currentGameTime) &&
            currentGameTime >= 0.0f &&
            currentGameTime < 100000.0f;
        const bool blowTimeValid =
            std::isfinite(bombBlowTime) &&
            gameTimeValid &&
            bombBlowTime > currentGameTime &&
            bombBlowTime < currentGameTime + 120.0f;
        const bool defuseTimeValid =
            std::isfinite(bombDefuseEndTime) &&
            gameTimeValid &&
            bombDefuseEndTime > currentGameTime &&
            bombDefuseEndTime < currentGameTime + 30.0f;

        if (!reusedConfirmedBombState) {
            s_bombState.blowTime = blowTimeValid ? bombBlowTime : 0.0f;
            s_bombState.defuseEndTime = defuseTimeValid ? bombDefuseEndTime : 0.0f;
            s_bombState.currentGameTime = gameTimeValid ? currentGameTime : 0.0f;
        }

        if (isValidWorldPos(s_bombState.position) && (s_bombState.planted || s_bombState.dropped)) {
            s_lastConfirmedBombState = s_bombState;
            s_lastConfirmedBombStateUs = nowUs;
        }
