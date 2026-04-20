        if (shouldScanWorld && worldScanCommitted) {
            
            
            for (int i = scannedMarkerCount; i < s_worldMarkerCount && i < 256; ++i)
                s_worldMarkers[i].valid = false;
            s_worldMarkerCount = scannedMarkerCount;
            for (int i = 0; i < scannedMarkerCount && i < 256; ++i)
                s_worldMarkers[i] = scannedMarkers[i];
        } else {
            for (int i = 0; i < s_worldMarkerCount; ++i) {
                if (s_worldMarkers[i].valid && s_worldMarkers[i].expiresUs < nowUs)
                    s_worldMarkers[i].valid = false;
            }
        }
