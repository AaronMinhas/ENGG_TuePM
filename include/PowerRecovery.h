#pragma once

#include <Preferences.h>
#include "BridgeSystemDefs.h"

/**
 * Lightweight power failure recovery system for ESP32
 * Uses NVS (Non-Volatile Storage) to persist critical system state
 * Automatically saves state on changes and restores on boot
 */
class PowerRecovery {
public:
    PowerRecovery();
    
    void begin();
    
    // Extended state data for recovery
    struct RecoveryData {
        BridgeState currentState;
        BridgeState previousState;
        uint8_t activeBoatSide;  // 0=UNKNOWN, 1=LEFT, 2=RIGHT
        bool boatCycleActive;
        unsigned long stateEntryTime;  // Relative to boot, for timeout tracking
        
        // Critical timing state
        unsigned long openingStateEntryTime;  // For boat passage timeout
        unsigned long boatClearanceTime;      // For 10-second clearance delay
        bool waitingToClearBeforeClose;
        bool oppositeSideDetectedDuringClearance;
        
        // Beam break and pending requests
        bool beamBreakActive;
        uint8_t pendingLowerRequest;  // 0=NONE, 1=AUTO, 2=MANUAL
    };
    
    // Save current state to NVS (called after any state change)
    void saveState(BridgeState currentState, BridgeState previousState);
    void saveExtendedState(const RecoveryData& data);
    
    // Check if valid recovery data exists
    bool hasRecoveryData() const;
    
    // Get recovered state
    BridgeState getRecoveredState() const;
    BridgeState getPreviousState() const;
    RecoveryData getRecoveryData() const;
    
    // Clear recovery data (call after successful recovery)
    void clearRecoveryData();
    
    // Get boot count (useful for detecting repeated crashes)
    uint32_t getBootCount() const;
    
    // Check if system is in crash loop (multiple rapid boots)
    bool isInCrashLoop() const;
    
private:
    Preferences nvs_;
    bool recoveryDataValid_;
    RecoveryData recoveryData_;
    uint32_t bootCount_;
    
    // NVS keys
    static constexpr const char* NVS_NAMESPACE = "bridge";
    static constexpr const char* KEY_CURRENT_STATE = "curr_state";
    static constexpr const char* KEY_PREV_STATE = "prev_state";
    static constexpr const char* KEY_BOAT_SIDE = "boat_side";
    static constexpr const char* KEY_CYCLE_ACTIVE = "cycle_act";
    static constexpr const char* KEY_STATE_TIME = "state_time";
    static constexpr const char* KEY_OPEN_TIME = "open_time";
    static constexpr const char* KEY_CLEAR_TIME = "clear_time";
    static constexpr const char* KEY_WAIT_CLEAR = "wait_clear";
    static constexpr const char* KEY_OPP_DETECT = "opp_detect";
    static constexpr const char* KEY_BEAM_ACTIVE = "beam_act";
    static constexpr const char* KEY_PENDING_REQ = "pend_req";
    static constexpr const char* KEY_VALID = "valid";
    static constexpr const char* KEY_BOOT_COUNT = "boot_cnt";
    static constexpr const char* KEY_LAST_BOOT_TIME = "last_boot";
};

