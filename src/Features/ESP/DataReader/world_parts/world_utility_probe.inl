        
        
        
        bool isUnknownUtilityProbe[kMaxTrackedWorldEntities + 1] = {};
        if (worldScanOk && shouldReadWorldUtilityProbeDetails) {
            for (int i = 0; i < probeQueuedCount; ++i)
                isUnknownUtilityProbe[s_probeQueuedIndices[i]] = true;
            if (probeQueuedCount > 0)
                s_lastWorldUtilityProbeScanUs = nowUs;
        }
