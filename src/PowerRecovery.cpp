#include "PowerRecovery.h"
#include "BridgeSystemDefs.h"
#include "Logger.h"

// Helper function to convert state to string (avoid circular dependency with BridgeStateMachine)
static const char* stateToString(BridgeState s) {
    switch (s) {
        case BridgeState::IDLE: return "IDLE";
        case BridgeState::STOPPING_TRAFFIC: return "STOPPING_TRAFFIC";
        case BridgeState::OPENING: return "OPENING";
        case BridgeState::OPEN: return "OPEN";
        case BridgeState::CLOSING: return "CLOSING";
        case BridgeState::RESUMING_TRAFFIC: return "RESUMING_TRAFFIC";
        case BridgeState::FAULT: return "FAULT";
        case BridgeState::MANUAL_MODE: return "MANUAL_MODE";
        case BridgeState::MANUAL_OPENING: return "MANUAL_OPENING";
        case BridgeState::MANUAL_OPEN: return "MANUAL_OPEN";
        case BridgeState::MANUAL_CLOSING: return "MANUAL_CLOSING";
        case BridgeState::MANUAL_CLOSED: return "MANUAL_CLOSED";
        default: return "UNKNOWN";
    }
}

PowerRecovery::PowerRecovery() 
    : recoveryDataValid_(false),
      bootCount_(0) {
    recoveryData_.currentState = BridgeState::IDLE;
    recoveryData_.previousState = BridgeState::IDLE;
    recoveryData_.activeBoatSide = 0;
    recoveryData_.boatCycleActive = false;
    recoveryData_.stateEntryTime = 0;
    recoveryData_.openingStateEntryTime = 0;
    recoveryData_.boatClearanceTime = 0;
    recoveryData_.waitingToClearBeforeClose = false;
    recoveryData_.oppositeSideDetectedDuringClearance = false;
    recoveryData_.beamBreakActive = false;
    recoveryData_.pendingLowerRequest = 0;
}

void PowerRecovery::begin() {
    // Open NVS in read-write mode
    if (!nvs_.begin(NVS_NAMESPACE, false)) {
        LOG_ERROR(Logger::TAG_SYS, "PowerRecovery: Failed to open NVS");
        return;
    }
    
    // Increment boot counter
    bootCount_ = nvs_.getUInt(KEY_BOOT_COUNT, 0) + 1;
    nvs_.putUInt(KEY_BOOT_COUNT, bootCount_);
    
    // Track boot time for crash loop detection
    unsigned long currentBootTime = millis();  // Will be near 0
    unsigned long lastBootTime = nvs_.getULong(KEY_LAST_BOOT_TIME, 0);
    nvs_.putULong(KEY_LAST_BOOT_TIME, currentBootTime);
    
    // Check if recovery data is valid
    recoveryDataValid_ = nvs_.getBool(KEY_VALID, false);
    
    if (recoveryDataValid_) {
        // Read saved state
        recoveryData_.currentState = static_cast<BridgeState>(
            nvs_.getUChar(KEY_CURRENT_STATE, static_cast<uint8_t>(BridgeState::IDLE)));
        recoveryData_.previousState = static_cast<BridgeState>(
            nvs_.getUChar(KEY_PREV_STATE, static_cast<uint8_t>(BridgeState::IDLE)));
        recoveryData_.activeBoatSide = nvs_.getUChar(KEY_BOAT_SIDE, 0);
        recoveryData_.boatCycleActive = nvs_.getBool(KEY_CYCLE_ACTIVE, false);
        recoveryData_.stateEntryTime = nvs_.getULong(KEY_STATE_TIME, 0);
        
        // Read extended timing state
        recoveryData_.openingStateEntryTime = nvs_.getULong(KEY_OPEN_TIME, 0);
        recoveryData_.boatClearanceTime = nvs_.getULong(KEY_CLEAR_TIME, 0);
        recoveryData_.waitingToClearBeforeClose = nvs_.getBool(KEY_WAIT_CLEAR, false);
        recoveryData_.oppositeSideDetectedDuringClearance = nvs_.getBool(KEY_OPP_DETECT, false);
        recoveryData_.beamBreakActive = nvs_.getBool(KEY_BEAM_ACTIVE, false);
        recoveryData_.pendingLowerRequest = nvs_.getUChar(KEY_PENDING_REQ, 0);
        
        LOG_WARN(Logger::TAG_SYS, "PowerRecovery: Valid recovery data found (Boot #%u)", bootCount_);
        LOG_WARN(Logger::TAG_SYS, "  Recovered State: %s", stateToString(recoveryData_.currentState));
        LOG_WARN(Logger::TAG_SYS, "  Previous State: %s", stateToString(recoveryData_.previousState));
        LOG_WARN(Logger::TAG_SYS, "  Boat Cycle Active: %s", recoveryData_.boatCycleActive ? "YES" : "NO");
        if (recoveryData_.boatCycleActive) {
            const char* sideStr = (recoveryData_.activeBoatSide == 1) ? "LEFT" : 
                                  (recoveryData_.activeBoatSide == 2) ? "RIGHT" : "UNKNOWN";
            LOG_WARN(Logger::TAG_SYS, "  Active Boat Side: %s", sideStr);
        }
        if (recoveryData_.waitingToClearBeforeClose) {
            LOG_WARN(Logger::TAG_SYS, "  Waiting for clearance delay");
        }
        if (recoveryData_.beamBreakActive) {
            LOG_WARN(Logger::TAG_SYS, "  Beam break was ACTIVE");
        }
        if (recoveryData_.pendingLowerRequest != 0) {
            LOG_WARN(Logger::TAG_SYS, "  Pending lower request: %s", 
                     recoveryData_.pendingLowerRequest == 1 ? "AUTO" : "MANUAL");
        }
    } else {
        LOG_INFO(Logger::TAG_SYS, "PowerRecovery: No valid recovery data (Boot #%u)", bootCount_);
    }
}

void PowerRecovery::saveState(BridgeState currentState, BridgeState previousState) {
    // Save basic state to NVS (fast, only writes if changed)
    nvs_.putUChar(KEY_CURRENT_STATE, static_cast<uint8_t>(currentState));
    nvs_.putUChar(KEY_PREV_STATE, static_cast<uint8_t>(previousState));
    nvs_.putBool(KEY_VALID, true);
    
    LOG_DEBUG(Logger::TAG_SYS, "PowerRecovery: State saved to NVS");
}

void PowerRecovery::saveExtendedState(const RecoveryData& data) {
    // Save extended state to NVS
    nvs_.putUChar(KEY_CURRENT_STATE, static_cast<uint8_t>(data.currentState));
    nvs_.putUChar(KEY_PREV_STATE, static_cast<uint8_t>(data.previousState));
    nvs_.putUChar(KEY_BOAT_SIDE, data.activeBoatSide);
    nvs_.putBool(KEY_CYCLE_ACTIVE, data.boatCycleActive);
    nvs_.putULong(KEY_STATE_TIME, data.stateEntryTime);
    
    // Save extended timing state
    nvs_.putULong(KEY_OPEN_TIME, data.openingStateEntryTime);
    nvs_.putULong(KEY_CLEAR_TIME, data.boatClearanceTime);
    nvs_.putBool(KEY_WAIT_CLEAR, data.waitingToClearBeforeClose);
    nvs_.putBool(KEY_OPP_DETECT, data.oppositeSideDetectedDuringClearance);
    nvs_.putBool(KEY_BEAM_ACTIVE, data.beamBreakActive);
    nvs_.putUChar(KEY_PENDING_REQ, data.pendingLowerRequest);
    
    nvs_.putBool(KEY_VALID, true);
    
    LOG_DEBUG(Logger::TAG_SYS, "PowerRecovery: Extended state saved to NVS");
}

bool PowerRecovery::hasRecoveryData() const {
    return recoveryDataValid_;
}

BridgeState PowerRecovery::getRecoveredState() const {
    return recoveryData_.currentState;
}

BridgeState PowerRecovery::getPreviousState() const {
    return recoveryData_.previousState;
}

PowerRecovery::RecoveryData PowerRecovery::getRecoveryData() const {
    return recoveryData_;
}

void PowerRecovery::clearRecoveryData() {
    nvs_.putBool(KEY_VALID, false);
    recoveryDataValid_ = false;
    LOG_INFO(Logger::TAG_SYS, "PowerRecovery: Recovery data cleared");
}

uint32_t PowerRecovery::getBootCount() const {
    return bootCount_;
}

bool PowerRecovery::isInCrashLoop() const {
    // Consider crash loop if:
    // 1. Recovery data exists (indicates power failure during operation)
    // 2. Boot count is high (>5 boots with recovery data present)
    // 3. In a non-idle operational state
    
    if (!recoveryDataValid_) {
        return false;  // Clean boot, not a crash
    }
    
    // If we've booted many times with recovery data, likely crash loop
    if (bootCount_ > 5) {
        LOG_WARN(Logger::TAG_SYS, "PowerRecovery: Crash loop detected - %u rapid boots", bootCount_);
        return true;
    }
    
    return false;
}

