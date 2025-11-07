import { useRef, useCallback } from "react";
import { BridgeStatus, CarTrafficStatus, BoatTrafficStatus, BridgeState, SensorStatus, SensorZone } from "../lib/schema";

/**
\ * Simulates the full cycle: IDLE -> STOPPING_TRAFFIC -> OPENING -> OPEN -> CLOSING -> RESUMING_TRAFFIC -> IDLE
 */

interface TestModeFSMState {
  currentState: BridgeState;
  boatCycleActive: boolean;
  activeBoatSide: "left" | "right" | null;
  timeoutRefs: ReturnType<typeof setTimeout>[];
  intervalRefs: ReturnType<typeof setInterval>[];
}

const TIMINGS = {
  TRAFFIC_STOP_DELAY: 2000,      
  BRIDGE_OPENING_DELAY: 3000,    
  BOAT_APPROACH_DELAY: 3000,     
  BOAT_PASSAGE_DELAY: 2000,     
  BOAT_DEPART_DELAY: 3000,       
  CLEARANCE_DELAY: 10000,         
  BRIDGE_CLOSING_DELAY: 3000,     
  TRAFFIC_RESUME_DELAY: 2000,     
  ULTRASONIC_UPDATE_INTERVAL: 200, 
};

export function useTestModeFSM(
  setBridgeStatus: React.Dispatch<React.SetStateAction<BridgeStatus | null>>,
  setCarTrafficStatus: React.Dispatch<React.SetStateAction<CarTrafficStatus | null>>,
  setBoatTrafficStatus: React.Dispatch<React.SetStateAction<BoatTrafficStatus | null>>,
  setSensorStatus: React.Dispatch<React.SetStateAction<SensorStatus | null>>,
  logActivity: (type: "sent" | "received", message: string) => void
) {
  const fsmStateRef = useRef<TestModeFSMState>({
    currentState: "IDLE",
    boatCycleActive: false,
    activeBoatSide: null,
    timeoutRefs: [],
    intervalRefs: [],
  });

  const clearAllTimeouts = useCallback(() => {
    if (!fsmStateRef.current) return;
    
    if (fsmStateRef.current.timeoutRefs && Array.isArray(fsmStateRef.current.timeoutRefs)) {
      fsmStateRef.current.timeoutRefs.forEach(timeout => clearTimeout(timeout));
      fsmStateRef.current.timeoutRefs = [];
    }
    
    if (fsmStateRef.current.intervalRefs && Array.isArray(fsmStateRef.current.intervalRefs)) {
      fsmStateRef.current.intervalRefs.forEach(interval => clearInterval(interval));
      fsmStateRef.current.intervalRefs = [];
    }
  }, []);

  const getZone = (distanceCm: number): SensorZone => {
    if (distanceCm < 30) return "close";
    if (distanceCm < 80) return "near";
    if (distanceCm < 200) return "far";
    return "none";
  };

  const updateUltrasonic = useCallback((
    side: "left" | "right",
    distanceCm: number,
    beamBreak: boolean = false,
    direction: "left-to-right" | "right-to-left" | "none" = "none"
  ) => {
    const zone = getZone(distanceCm);
    setSensorStatus(prev => ({
      leftUltrasonic: side === "left" ? {
        distanceCm: Math.round(distanceCm),
        zone,
      } : prev?.leftUltrasonic || {
        distanceCm: 200,
        zone: "far",
      },
      rightUltrasonic: side === "right" ? {
        distanceCm: Math.round(distanceCm),
        zone,
      } : prev?.rightUltrasonic || {
        distanceCm: 200,
        zone: "far",
      },
      beamBreak,
      direction,
      receivedAt: Date.now(),
    }));
  }, [setSensorStatus]);

  const changeState = useCallback((newState: BridgeState, side?: "left" | "right") => {
    const prevState = fsmStateRef.current.currentState;
    fsmStateRef.current.currentState = newState;
    
    const now = Date.now();
    setBridgeStatus(prev => prev ? {
      ...prev,
      state: newState,
      lastChangeMs: now,
      receivedAt: now,
    } : {
      state: newState,
      lastChangeMs: now,
      lockEngaged: newState === "IDLE" || newState === "RESUMING_TRAFFIC",
      receivedAt: now,
    });

    logActivity("received", `FSM: State changed from ${prevState} to ${newState}${side ? ` (boat from ${side})` : ""}`);
  }, [setBridgeStatus, logActivity]);

  // Start boat detection 
  const startBoatCycle = useCallback((side: "left" | "right") => {
    if (fsmStateRef.current.currentState !== "IDLE" || fsmStateRef.current.boatCycleActive) {
      logActivity("received", `FSM: Boat detected from ${side} but cycle already active or not in IDLE - ignoring`);
      return;
    }

    clearAllTimeouts();
    fsmStateRef.current.boatCycleActive = true;
    fsmStateRef.current.activeBoatSide = side;

    logActivity("received", `FSM: Boat detected from ${side} - starting bridge cycle`);

    const initialDistance = 60;
    const boatDirection = side === "left" ? "left-to-right" : "right-to-left";
    updateUltrasonic(side, initialDistance, false, boatDirection);
    logActivity("received", `FSM: Ultrasonic ${side} sensor detected boat at ${initialDistance}cm (near zone)`);

    // Step 1: IDLE -> STOPPING_TRAFFIC
    changeState("STOPPING_TRAFFIC", side);
    
    // Set car lights to red
    setCarTrafficStatus({
      left: { value: "Red", receivedAt: Date.now() },
      right: { value: "Red", receivedAt: Date.now() },
    });
    logActivity("received", "FSM: Car traffic lights set to RED - waiting for pedestrians to clear");

    // Simulate boat approaching: ultrasonic distance decreasing
    let currentDistance = initialDistance;
    const approachSteps = TIMINGS.BOAT_APPROACH_DELAY / TIMINGS.ULTRASONIC_UPDATE_INTERVAL;
    const distanceDecrease = (initialDistance - 25) / approachSteps;
    
    const approachInterval = setInterval(() => {
      currentDistance -= distanceDecrease;
      if (currentDistance <= 25) {
        currentDistance = 25;
        clearInterval(approachInterval);
        if (fsmStateRef.current?.intervalRefs) {
          const idx = fsmStateRef.current.intervalRefs.indexOf(approachInterval);
          if (idx > -1) fsmStateRef.current.intervalRefs.splice(idx, 1);
        }
      }
      updateUltrasonic(side, currentDistance, false, boatDirection);
    }, TIMINGS.ULTRASONIC_UPDATE_INTERVAL);

    if (fsmStateRef.current.intervalRefs) {
      fsmStateRef.current.intervalRefs.push(approachInterval);
    }

    // Store timeout to clear interval
    const approachTimeout = setTimeout(() => {
      clearInterval(approachInterval);
      if (fsmStateRef.current?.intervalRefs) {
        const idx = fsmStateRef.current.intervalRefs.indexOf(approachInterval);
        if (idx > -1) fsmStateRef.current.intervalRefs.splice(idx, 1);
      }
    }, TIMINGS.BOAT_APPROACH_DELAY);
    
    if (fsmStateRef.current.timeoutRefs) {
      fsmStateRef.current.timeoutRefs.push(approachTimeout);
    }

    // Step 2: After traffic stop delay -> OPENING
    const openingTimeout = setTimeout(() => {
      changeState("OPENING", side);
      logActivity("received", "FSM: Traffic stopped - bridge opening");

      // Step 3: After opening delay -> OPEN
      const openTimeout = setTimeout(() => {
        changeState("OPEN", side);
        
        // Set boat light to green for detected side
        setBoatTrafficStatus(prev => ({
          left: side === "left" ? { value: "Green", receivedAt: Date.now() } : prev?.left || { value: "Red", receivedAt: Date.now() },
          right: side === "right" ? { value: "Green", receivedAt: Date.now() } : prev?.right || { value: "Red", receivedAt: Date.now() },
        }));
        logActivity("received", `FSM: Bridge opened - boat light GREEN for ${side} side`);

        // Step 4: Simulate boat passing beam break
        const boatPassTimeout = setTimeout(() => {
          // Boat is at beam break - activate beam break sensor
          updateUltrasonic(side, 20, true, boatDirection);
          logActivity("received", "FSM: Boat at beam break - beam break sensor ACTIVE");
          
          // After a short delay, boat passes through
          const beamBreakTimeout = setTimeout(() => {
            logActivity("received", "FSM: Boat passed beam break - boat has cleared channel");
            
            // Clear beam break immediately after boat passes
            updateUltrasonic(side, 20, false, boatDirection);
            
            // Turn boat lights red
            setBoatTrafficStatus({
              left: { value: "Red", receivedAt: Date.now() },
              right: { value: "Red", receivedAt: Date.now() },
            });

            // Simulate boat departing: ultrasonic distance increasing on opposite side
            const oppositeSide = side === "left" ? "right" : "left";
            let departDistance = 25;
            const departSteps = TIMINGS.BOAT_DEPART_DELAY / TIMINGS.ULTRASONIC_UPDATE_INTERVAL;
            const distanceIncrease = (60 - 25) / departSteps;
            
            const departInterval = setInterval(() => {
              departDistance += distanceIncrease;
              if (departDistance >= 60) {
                departDistance = 60;
                clearInterval(departInterval);
                if (fsmStateRef.current?.intervalRefs) {
                  const idx = fsmStateRef.current.intervalRefs.indexOf(departInterval);
                  if (idx > -1) fsmStateRef.current.intervalRefs.splice(idx, 1);
                }
                setTimeout(() => {
                  updateUltrasonic(oppositeSide, 60, false, "none");
                  logActivity("received", `FSM: Boat departed - ${oppositeSide} ultrasonic back to max distance (60cm)`);
                }, 500);
              }
              updateUltrasonic(oppositeSide, departDistance, false, boatDirection);
            }, TIMINGS.ULTRASONIC_UPDATE_INTERVAL);

            if (fsmStateRef.current.intervalRefs) {
              fsmStateRef.current.intervalRefs.push(departInterval);
            }

            const departTimeout = setTimeout(() => {
              clearInterval(departInterval);
              if (fsmStateRef.current?.intervalRefs) {
                const idx = fsmStateRef.current.intervalRefs.indexOf(departInterval);
                if (idx > -1) fsmStateRef.current.intervalRefs.splice(idx, 1);
              }
            }, TIMINGS.BOAT_DEPART_DELAY);
            
            if (fsmStateRef.current.timeoutRefs) {
              fsmStateRef.current.timeoutRefs.push(departTimeout);
            }

            // Step 5: Start clearance delay (10 seconds)
            logActivity("received", "FSM: Starting 10-second clearance delay - monitoring opposite side");
            
            const clearanceTimeout = setTimeout(() => {
            // Simulate opposite side detection during clearance (normal case)
            logActivity("received", `FSM: Opposite side (${side === "left" ? "right" : "left"}) detected boat during clearance - proceeding to close`);

            // Step 6: After clearance -> CLOSING
            changeState("CLOSING", side);
            logActivity("received", "FSM: Clearance delay complete - bridge closing");

            // Step 7: After closing delay -> RESUMING_TRAFFIC
            const closingTimeout = setTimeout(() => {
              changeState("RESUMING_TRAFFIC", side);
              logActivity("received", "FSM: Bridge closed - resuming traffic");

              // Step 8: After resume delay -> IDLE
              const resumeTimeout = setTimeout(() => {
                changeState("IDLE");
                
                // Set car lights back to green
                setCarTrafficStatus({
                  left: { value: "Green", receivedAt: Date.now() },
                  right: { value: "Green", receivedAt: Date.now() },
                });
                
                // Reset FSM state
                fsmStateRef.current.boatCycleActive = false;
                fsmStateRef.current.activeBoatSide = null;
                
                updateUltrasonic("left", 60, false, "none");
                updateUltrasonic("right", 60, false, "none");
                
                logActivity("received", "FSM: Traffic resumed - bridge cycle complete, returning to IDLE");
              }, TIMINGS.TRAFFIC_RESUME_DELAY);

              if (fsmStateRef.current.timeoutRefs) {
                fsmStateRef.current.timeoutRefs.push(resumeTimeout);
              }
            }, TIMINGS.BRIDGE_CLOSING_DELAY);

            if (fsmStateRef.current.timeoutRefs) {
              fsmStateRef.current.timeoutRefs.push(closingTimeout);
            }
          }, TIMINGS.CLEARANCE_DELAY);

            if (fsmStateRef.current.timeoutRefs) {
              fsmStateRef.current.timeoutRefs.push(clearanceTimeout);
            }
          }, TIMINGS.BOAT_PASSAGE_DELAY);
            
          if (fsmStateRef.current.timeoutRefs) {
            fsmStateRef.current.timeoutRefs.push(beamBreakTimeout);
          }
        }, TIMINGS.BOAT_PASSAGE_DELAY);

        if (fsmStateRef.current.timeoutRefs) {
          fsmStateRef.current.timeoutRefs.push(boatPassTimeout);
        }
      }, TIMINGS.BRIDGE_OPENING_DELAY);

      if (fsmStateRef.current.timeoutRefs) {
        fsmStateRef.current.timeoutRefs.push(openTimeout);
      }
    }, TIMINGS.TRAFFIC_STOP_DELAY);

    if (fsmStateRef.current.timeoutRefs) {
      fsmStateRef.current.timeoutRefs.push(openingTimeout);
    }
  }, [changeState, setCarTrafficStatus, setBoatTrafficStatus, updateUltrasonic, logActivity, clearAllTimeouts]);

  // Handle boat passed command (simulates beam break)
  const handleBoatPassed = useCallback(() => {
    if (fsmStateRef.current.currentState === "OPEN" && fsmStateRef.current.boatCycleActive) {
      // This will be handled by the timeout, but we can trigger it early
      logActivity("received", "FSM: Boat passed command received (beam break simulated)");
      // The timeout will handle the rest
    } else {
      logActivity("received", "FSM: Boat passed command ignored - not in OPEN state");
    }
  }, [logActivity]);

  // Reset FSM to IDLE
  const resetFSM = useCallback(() => {
    clearAllTimeouts();
    fsmStateRef.current.boatCycleActive = false;
    fsmStateRef.current.activeBoatSide = null;
    changeState("IDLE");
    
    // Reset traffic lights
    setCarTrafficStatus({
      left: { value: "Green", receivedAt: Date.now() },
      right: { value: "Green", receivedAt: Date.now() },
    });
    
    setBoatTrafficStatus({
      left: { value: "Red", receivedAt: Date.now() },
      right: { value: "Red", receivedAt: Date.now() },
    });
    
    updateUltrasonic("left", 60, false, "none");
    updateUltrasonic("right", 60, false, "none");
    
    logActivity("received", "FSM: Reset to IDLE state - sensors reset");
  }, [clearAllTimeouts, changeState, setCarTrafficStatus, setBoatTrafficStatus, updateUltrasonic, logActivity]);

  return {
    startBoatCycle,
    handleBoatPassed,
    resetFSM,
    getCurrentState: () => fsmStateRef.current.currentState,
    isBoatCycleActive: () => fsmStateRef.current.boatCycleActive,
  };
}

