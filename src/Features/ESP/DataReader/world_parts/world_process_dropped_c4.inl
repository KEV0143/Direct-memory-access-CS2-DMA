            
            
            
            if (rawItemId == kWeaponC4Id && worldSceneNodes[idx] != 0 && posNonOrigin) {
                rememberWorldBombCandidateSlot(idx);
                const bool c4OwnerAlive =
                    ownerPlayerIndex >= 0 && ownerPlayerIndex < 64 &&
                    healths[ownerPlayerIndex] > 0 &&
                    lifeStates[ownerPlayerIndex] == 0;
                int c4CandidateScore = 0;
                if (bombDroppedByRules)
                    c4CandidateScore += 180;
                if (noOwner)
                    c4CandidateScore += 220;
                if (ownerPlayerIndex < 0)
                    c4CandidateScore += 120;
                if (!c4OwnerAlive)
                    c4CandidateScore += 100;
                if (!ownerHoldingNearby)
                    c4CandidateScore += 80;
                if (ownerActiveWeaponMatches)
                    c4CandidateScore -= 220;
                if (c4OwnerAlive && ownerHoldingNearby)
                    c4CandidateScore -= 120;
                if (s_bombState.dropped && isFiniteVec(s_bombState.position)) {
                    const float c4Dx = pos.x - s_bombState.position.x;
                    const float c4Dy = pos.y - s_bombState.position.y;
                    if ((c4Dx * c4Dx + c4Dy * c4Dy) <= (160.0f * 160.0f))
                        c4CandidateScore += 45;
                }

                if (!worldScanFoundC4 || c4CandidateScore > worldScanC4Score) {
                    worldScanFoundC4 = true;
                    worldScanC4Score = c4CandidateScore;
                    worldScanC4Entity = ent;
                    worldScanC4Pos = pos;
                    worldScanC4OwnerIdx = ownerPlayerIndex;
                    worldScanC4NoOwner = noOwner;
                    worldScanC4OwnerAlive = c4OwnerAlive;
                    worldScanC4OwnerNearby = ownerHoldingNearby;
                }
            }

            if (droppedItemCandidate &&
                droppedOwnerReleased &&
                worldSceneNodes[idx] != 0 &&
                posNonOrigin) {
                pushWorldMarker(WorldMarkerType::DroppedWeapon, pos, itemId, 0.0f, nowUs + 400000);
            }
