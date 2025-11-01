/**
 * This component will be in the centre of the dashboard.
 * Its purpose is to display the bridges current state in an interesting way for the user, such as
 * showing a "bridge" open or closed based on the state.
 */

import React, { useEffect, useState, useRef } from "react";
import type { BridgeStatus, CarTrafficStatus, BoatTrafficStatus } from "../lib/schema";

interface BridgeCardProps {
  bridgeStatus?: BridgeStatus | null;
  carTrafficStatus?: CarTrafficStatus | null;
  boatTrafficStatus?: BoatTrafficStatus | null;
}

// Traffic Light Component for Cars (Red, Yellow, Green)
function TrafficLight({ red, yellow, green }: { red: boolean; yellow: boolean; green: boolean }) {
  return (
    <div className="traffic-light">
      <div className={`light red ${red ? "active" : ""}`}></div>
      <div className={`light yellow ${yellow ? "active" : ""}`}></div>
      <div className={`light green ${green ? "active" : ""}`}></div>
    </div>
  );
}

// Traffic Light Component for Boats (Red, Green only)
function BoatTrafficLight({ red, green }: { red: boolean; green: boolean }) {
  return (
    <div className="boat-traffic-light">
      <div className={`light red ${red ? "active" : ""}`}></div>
      <div className={`light green ${green ? "active" : ""}`}></div>
    </div>
  );
}

// Boat timer countdown component
function BoatTimerDisplay({ timerStartMs, side }: { timerStartMs: number; side: string }) {
  const [remainingSeconds, setRemainingSeconds] = useState<number>(45);
  const timerStartTimeRef = useRef<number | null>(null);
  const lastTimerStartMsRef = useRef<number>(0);
  
  useEffect(() => {
    // When timerStartMs changes to a new value, it means a new timer started
    // Reset our tracking when we see a different timer start value
    if (timerStartMs > 0 && timerStartMs !== lastTimerStartMsRef.current) {
      timerStartTimeRef.current = Date.now();
      lastTimerStartMsRef.current = timerStartMs;
      setRemainingSeconds(45);
    }
    
    // Reset if timer stopped
    if (timerStartMs === 0) {
      timerStartTimeRef.current = null;
      lastTimerStartMsRef.current = 0;
      setRemainingSeconds(45);
      return;
    }
    
    const updateRemaining = () => {
      if (timerStartTimeRef.current === null) return;
      
      // Calculate elapsed time from when frontend received timer start
      const now = Date.now();
      const elapsed = (now - timerStartTimeRef.current) / 1000;
      const remaining = Math.max(0, 45 - elapsed);
      setRemainingSeconds(Math.ceil(remaining));
    };
    
    updateRemaining();
    const interval = setInterval(updateRemaining, 100);
    
    return () => clearInterval(interval);
  }, [timerStartMs]);
  
  if (remainingSeconds <= 0 || !timerStartMs || timerStartMs === 0) return null;
  
  return (
    <div className="text-xs text-blue-600 mt-0.5 font-mono font-semibold">
      Boat window: {remainingSeconds}s ({side})
    </div>
  );
}

// Pedestrian timer countdown component
function PedestrianTimerDisplay({
  active,
  remainingMs,
}: {
  active: boolean;
  remainingMs: number;
}) {
  const [displayMs, setDisplayMs] = useState<number>(remainingMs);
  const [hasReceivedData, setHasReceivedData] = useState<boolean>(false);
  const originMsRef = useRef<number>(remainingMs);
  const originTimeRef = useRef<number>(performance.now());

  useEffect(() => {
    if (!active) {
      originMsRef.current = 0;
      setDisplayMs(0);
      setHasReceivedData(false);
      return;
    }

    if (remainingMs > 0) {
      originMsRef.current = remainingMs;
      originTimeRef.current = performance.now();
      setDisplayMs(remainingMs);
      setHasReceivedData(true); // Mark that we've received valid data
    }
  }, [active, remainingMs]);

  useEffect(() => {
    const tick = () => {
      if (!active || originMsRef.current <= 0) {
        setDisplayMs(0);
        return;
      }

      const elapsed = performance.now() - originTimeRef.current;
      const updated = Math.max(0, originMsRef.current - elapsed);
      setDisplayMs(updated);
    };

    const interval = setInterval(tick, 100);
    return () => clearInterval(interval);
  }, [active]);

  const remainingSeconds = Math.max(0, Math.ceil(displayMs / 1000));

  // Don't show until we've received valid timer data from backend
  if (!hasReceivedData || (!active && remainingSeconds <= 0)) return null;

  return (
    <div className="text-xs text-amber-600 mt-1 font-mono font-semibold">
      Pedestrians crossing: {remainingSeconds}s remaining
    </div>
  );
}

export default function BridgeCard({ bridgeStatus, carTrafficStatus, boatTrafficStatus }: BridgeCardProps) {
  const bridgeState = bridgeStatus?.state || "IDLE";
  const lockEngaged = bridgeStatus?.lockEngaged || false;
  const prevStateRef = useRef<string>(bridgeState);
  const [isAnimating, setIsAnimating] = useState(false);
  const [lastStateChangeTime, setLastStateChangeTime] = useState<number | null>(null);
  
  useEffect(() => {
    const prevState = prevStateRef.current;
    const currentState = bridgeState;
    
    // Detect state changes that should trigger animation
    if (prevState !== currentState) {
      // Update timestamp when state actually changes
      setLastStateChangeTime(Date.now());
      
      const isNowOpen = currentState === "OPEN" || currentState === "MANUAL_OPEN";
      const wasOpen = prevState === "OPEN" || prevState === "MANUAL_OPEN";
      const wasOpening = prevState === "OPENING" || prevState === "MANUAL_OPENING";
      const wasClosing = prevState === "CLOSING" || prevState === "MANUAL_CLOSING" || prevState === "RESUMING_TRAFFIC";
      const isNowClosed = currentState === "IDLE" || currentState === "CLOSED" || currentState.includes("CLOSED") || currentState.includes("MANUAL_CLOSED");
      
      // If transitioning TO OPEN from IDLE/CLOSED (skipping OPENING state - manual command)
      // Don't animate if coming from OPENING/MANUAL_OPENING (already animating)
      if (isNowOpen && !wasOpen && !wasOpening && currentState !== "OPENING" && currentState !== "MANUAL_OPENING") {
        setIsAnimating(true);
        setTimeout(() => setIsAnimating(false), 2000);
      }
      // If transitioning TO CLOSED/IDLE from OPEN (skipping CLOSING state - manual command)
      // Don't animate if coming from CLOSING/MANUAL_CLOSING (already animating)
      else if (isNowClosed && wasOpen && !wasClosing && currentState !== "CLOSING" && currentState !== "MANUAL_CLOSING" && currentState !== "RESUMING_TRAFFIC") {
        setIsAnimating(true);
        setTimeout(() => setIsAnimating(false), 2000);
      }
      
      prevStateRef.current = currentState;
    }
  }, [bridgeState]);
  
  const getBridgeClasses = () => {
    const baseClass = (() => {
      switch (bridgeState) {
        case "OPEN":
        case "MANUAL_OPEN":
          return "bridge-open";
        case "OPENING":
        case "MANUAL_OPENING":
          return "bridge-opening";
        case "CLOSING":
        case "MANUAL_CLOSING":
        case "RESUMING_TRAFFIC":
          return "bridge-closing";
        case "FAULT":
          return "bridge-error";
        default:
          return "bridge-closed";
      }
    })();
    
    // Add animation trigger class if manually transitioning
    if (isAnimating) {
      if (bridgeState === "OPEN" || bridgeState === "MANUAL_OPEN") {
        return `${baseClass} bridge-animate-open`;
      } else if (bridgeState === "IDLE" || bridgeState === "CLOSED" || bridgeState.includes("CLOSED")) {
        return `${baseClass} bridge-animate-close`;
      }
    }
    
    return baseClass;
  };

  const getStatusText = () => {
    switch (bridgeState) {
      case "IDLE":
        return "Idle";
      case "STOPPING_TRAFFIC":
        return "Stopping Traffic...";
      case "OPENING":
        return "Opening...";
      case "OPEN":
        return "Open";
      case "CLOSING":
        return "Closing...";
      case "RESUMING_TRAFFIC":
        return "Resuming Traffic...";
      case "FAULT":
        return "Fault Detected!";
      case "MANUAL_MODE":
        return "Manual Mode";
      case "MANUAL_OPENING":
        return "Manual Opening...";
      case "MANUAL_OPEN":
        return "Manual Open";
      case "MANUAL_CLOSING":
        return "Manual Closing...";
      case "MANUAL_CLOSED":
        return "Manual Closed";
      default:
        return bridgeState;
    }
  };

  const getStatusColor = () => {
    switch (bridgeState) {
      case "IDLE":
        return "text-green-600";
      case "OPEN":
      case "MANUAL_OPEN":
        return "text-green-500";
      case "STOPPING_TRAFFIC":
      case "OPENING":
      case "CLOSING":
      case "RESUMING_TRAFFIC":
        return "text-yellow-600";
      case "MANUAL_MODE":
      case "MANUAL_OPENING":
      case "MANUAL_CLOSING":
      case "MANUAL_CLOSED":
        return "text-blue-600";
      case "FAULT":
        return "text-red-600";
      default:
        return "text-gray-600";
    }
  };

  const pedestrianTimerStartMs = bridgeStatus?.pedestrianTimerStartMs ?? 0;
  const pedestrianTimerRemainingMs = bridgeStatus?.pedestrianTimerRemainingMs ?? 0;
  const pedestrianTimerActive =
    bridgeState === "STOPPING_TRAFFIC" ||
    (pedestrianTimerStartMs > 0 && pedestrianTimerRemainingMs > 0);
  const boatTimerStartMs = bridgeStatus?.boatTimerStartMs ?? 0;
  const boatTimerSide = bridgeStatus?.boatTimerSide || "";

  return (
    <div className="bg-white rounded-md p-4 h-full flex flex-col border-base-400 shadow-[0_0_2px_rgba(0,0,0,0.25)] border relative overflow-hidden">
      {/* Bridge Visual */}
      <div className="flex-1 flex items-center justify-center relative">
        <div className="bridge-container">
          {/* Water */}
          <div className="water"></div>
          
          {/* Bridge Structure */}
          <div className={`bridge ${getBridgeClasses()}`}>
            {/* Stationary Base Supports */}
            <div className="center-support left-support"></div>
            <div className="center-support right-support"></div>
            
            {/* Stationary Road Sections */}
            <div className="stationary-road left-road"></div>
            <div className="stationary-road right-road"></div>
            
            {/* Rotating Bridge Halves */}
            <div className="bridge-half left-half">
              <div className="bridge-deck"></div>
              <div className="bridge-cable"></div>
            </div>
            
            <div className="bridge-half right-half">
              <div className="bridge-deck"></div>
              <div className="bridge-cable"></div>
            </div>
          </div>
          
          {/* Lock Indicator */}
          {lockEngaged && (
            <div className="lock-indicator">
              <div className="lock-icon">LOCK</div>
            </div>
          )}
          
          {/* Error Indicator */}
          {bridgeState === "FAULT" && (
            <div className="error-indicator">
              <div className="error-icon">!</div>
            </div>
          )}
          
          {/* Car Traffic Lights */}
          <div className="car-traffic-lights left-car-light">
            <TrafficLight
              red={carTrafficStatus?.left.value === "Red"}
              yellow={carTrafficStatus?.left.value === "Yellow"}
              green={carTrafficStatus?.left.value === "Green"}
            />
          </div>
          <div className="car-traffic-lights right-car-light">
            <TrafficLight
              red={carTrafficStatus?.right.value === "Red"}
              yellow={carTrafficStatus?.right.value === "Yellow"}
              green={carTrafficStatus?.right.value === "Green"}
            />
          </div>
          
          {/* Boat Traffic Lights */}
          <div className="boat-traffic-lights left-boat-light">
            <BoatTrafficLight
              red={boatTrafficStatus?.left.value === "Red"}
              green={boatTrafficStatus?.left.value === "Green"}
            />
          </div>
          <div className="boat-traffic-lights right-boat-light">
            <BoatTrafficLight
              red={boatTrafficStatus?.right.value === "Red"}
              green={boatTrafficStatus?.right.value === "Green"}
            />
          </div>
          
        </div>
      </div>
      
      {/* Status Text */}
      <div className="text-center mt-2">
        <div className={`text-lg font-semibold ${getStatusColor()}`}>
          {getStatusText()}
        </div>
        {lastStateChangeTime && (
          <div className="text-xs text-gray-500 mt-0.5">
            Last changed: {new Date(lastStateChangeTime).toLocaleTimeString()}
          </div>
        )}
        {pedestrianTimerActive && (
          <PedestrianTimerDisplay 
            active={bridgeState === "STOPPING_TRAFFIC"}
            remainingMs={pedestrianTimerRemainingMs}
          />
        )}
        {bridgeStatus?.boatTimerStartMs && bridgeStatus.boatTimerStartMs > 0 && (
          <BoatTimerDisplay 
            timerStartMs={bridgeStatus.boatTimerStartMs} 
            side={bridgeStatus.boatTimerSide || ""}
          />
        )}
      </div>
      
      {/* Custom CSS for animations */}
      <style>{`
        .bridge-container {
          width: 600px;
          height: 280px;
          position: relative;
          display: flex;
          align-items: center;
          justify-content: center;
        }
        
        .water {
          position: absolute;
          bottom: 0;
          left: 0;
          right: 0;
          height: 60px;
          background: linear-gradient(to bottom, #3b82f6, #1e40af);
          border-radius: 0 0 8px 8px;
        }
        
        .bridge {
          position: relative;
          width: 480px;
          height: 130px;
          display: flex;
          align-items: center;
          justify-content: center;
          transition: all 0.8s cubic-bezier(0.4, 0, 0.2, 1);
        }
        
        /* Bridge Supports */
        .center-support {
          position: absolute;
          bottom: -22px;
          width: 18px;
          height: 45px;
          background: #374151;
          border-radius: 3px;
          z-index: 2;
          box-shadow: 0 3px 6px rgba(0, 0, 0, 0.3);
        }
        
        .left-support {
          left: 75px;
          transform: translateX(-50%);
        }
        
        .right-support {
          right: 75px;
          transform: translateX(50%);
        }
        
        .center-support::before {
          content: '';
          position: absolute;
          top: -12px;
          left: -4px;
          right: -4px;
          height: 12px;
          background: #374151;
          border-radius: 3px;
        }
        
        /* Stationary Road Sections */
        .stationary-road {
          position: absolute;
          height: 30px;
          background: linear-gradient(to bottom, #6b7280, #4b5563);
          border-radius: 6px;
          box-shadow: 0 3px 6px rgba(0, 0, 0, 0.3);
          z-index: 1;
        }
        
        .stationary-road::before {
          content: '';
          position: absolute;
          top: 3px;
          left: 3px;
          right: 3px;
          height: 3px;
          background: #9ca3af;
        }
        
        .left-road {
          left: 30px;
          top: 65px;
          width: 105px;
        }
        
        .right-road {
          right: 30px;
          top: 65px;
          width: 105px;
        }
        
        /* Rotating Bridge Halves */
        .bridge-half {
          position: absolute;
          width: 105px;
          height: 90px;
          transition: transform 2s cubic-bezier(0.4, 0, 0.2, 1);
          z-index: 3;
          will-change: transform; /* Optimize animations */
        }
        
        .left-half {
          left: 135px;
          top: 65px;
          transform-origin: 0% 50%;
          transform: rotate(0deg); /* Default state */
        }
        
        .right-half {
          left: 240px;
          top: 65px;
          transform-origin: 100% 50%;
          transform: rotate(0deg); /* Default state */
        }
        
        .bridge-deck {
          width: calc(100% + 4px);
          height: 30px;
          background: linear-gradient(to bottom, #6b7280, #4b5563);
          border-radius: 6px;
          position: relative;
          box-shadow: 0 3px 6px rgba(0, 0, 0, 0.3);
          z-index: 3;
          margin-left: -2px;
          margin-right: -2px;
        }
        
        .bridge-deck::before {
          content: '';
          position: absolute;
          top: 3px;
          left: 3px;
          right: 3px;
          height: 3px;
          background: #9ca3af;
        }
        
        /* Bridge States - Use transitions for smooth animations on all state changes */
        .bridge-closed .left-half {
          transform: rotate(0deg);
        }
        
        .bridge-closed .right-half {
          transform: rotate(0deg);
        }
        
        /* During OPENING state, use animation for smooth visual feedback */
        .bridge-opening .left-half {
          animation: openLeft 2s ease-in-out forwards;
        }
        
        .bridge-opening .right-half {
          animation: openRight 2s ease-in-out forwards;
        }
        
        /* Final OPEN state - use animation when manually triggered, transition otherwise */
        .bridge-open:not(.bridge-animate-open) .left-half {
          transform: rotate(-45deg);
        }
        
        .bridge-open:not(.bridge-animate-open) .right-half {
          transform: rotate(45deg);
        }
        
        /* Animate when manually transitioning to OPEN - animation takes precedence */
        .bridge-animate-open .left-half {
          transform: rotate(0deg); /* Reset to start position */
          animation: openLeft 2s ease-in-out forwards;
        }
        
        .bridge-animate-open .right-half {
          transform: rotate(0deg); /* Reset to start position */
          animation: openRight 2s ease-in-out forwards;
        }
        
        /* During CLOSING state, use animation for smooth visual feedback */
        .bridge-closing .left-half {
          animation: closeLeft 2s ease-in-out forwards;
        }
        
        .bridge-closing .right-half {
          animation: closeRight 2s ease-in-out forwards;
        }
        
        /* Animate when manually transitioning to CLOSED - animation takes precedence */
        .bridge-animate-close .left-half {
          transform: rotate(-45deg); /* Reset to start position */
          animation: closeLeft 2s ease-in-out forwards;
        }
        
        .bridge-animate-close .right-half {
          transform: rotate(45deg); /* Reset to start position */
          animation: closeRight 2s ease-in-out forwards;
        }
        
        .bridge-error {
          animation: errorShake 0.5s ease-in-out infinite;
        }
        
        /* Animations */
        @keyframes openLeft {
          from { transform: rotate(0deg); }
          to { transform: rotate(-45deg); }
        }
        
        @keyframes openRight {
          from { transform: rotate(0deg); }
          to { transform: rotate(45deg); }
        }
        
        @keyframes closeLeft {
          from { transform: rotate(-45deg); }
          to { transform: rotate(0deg); }
        }
        
        @keyframes closeRight {
          from { transform: rotate(45deg); }
          to { transform: rotate(0deg); }
        }
        
        @keyframes errorShake {
          0%, 100% { transform: translateX(0); }
          25% { transform: translateX(-5px); }
          75% { transform: translateX(5px); }
        }
        
        .lock-indicator {
          position: absolute;
          top: 10px;
          right: 10px;
          background: rgba(255, 255, 255, 0.9);
          border-radius: 50%;
          width: 30px;
          height: 30px;
          display: flex;
          align-items: center;
          justify-content: center;
          box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
        }
        
        .lock-icon {
          font-size: 10px;
          font-weight: bold;
          color: #374151;
        }
        
        .error-indicator {
          position: absolute;
          top: 10px;
          left: 10px;
          background: rgba(239, 68, 68, 0.9);
          border-radius: 50%;
          width: 30px;
          height: 30px;
          display: flex;
          align-items: center;
          justify-content: center;
          box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
        }
        
        .error-icon {
          font-size: 16px;
          font-weight: bold;
          color: white;
        }
        
        /* Traffic Lights */
        .car-traffic-lights,
        .boat-traffic-lights {
          position: absolute;
          z-index: 10;
        }
        
        .left-car-light {
          top: 30px;
          left: 30px;
        }
        
        .right-car-light {
          top: 30px;
          right: 30px;
        }
        
        .left-boat-light {
          bottom: 30px;
          left: 30px;
        }
        
        .right-boat-light {
          bottom: 30px;
          right: 30px;
        }
        
        /* Car Traffic Light (3 lights: Red, Yellow, Green) */
        .traffic-light {
          display: flex;
          flex-direction: column;
          align-items: center;
          gap: 6px;
          padding: 9px;
          background: #1f2937;
          border-radius: 12px;
          box-shadow: 0 3px 9px rgba(0, 0, 0, 0.3);
        }
        
        .traffic-light .light {
          width: 24px;
          height: 24px;
          border-radius: 50%;
          background: #374151;
          border: 3px solid #111827;
          transition: background-color 0.3s ease, box-shadow 0.3s ease, opacity 0.3s ease;
          opacity: 0.3;
        }
        
        .traffic-light .light.active {
          opacity: 1;
          box-shadow: 0 0 12px currentColor, inset 0 0 6px rgba(255, 255, 255, 0.3);
        }
        
        .traffic-light .light.red {
          background: #dc2626;
        }
        
        .traffic-light .light.red.active {
          background: #ef4444;
          box-shadow: 0 0 16px #ef4444, inset 0 0 8px rgba(255, 255, 255, 0.4);
        }
        
        .traffic-light .light.yellow {
          background: #d97706;
        }
        
        .traffic-light .light.yellow.active {
          background: #f59e0b;
          box-shadow: 0 0 16px #f59e0b, inset 0 0 8px rgba(255, 255, 255, 0.4);
        }
        
        .traffic-light .light.green {
          background: #16a34a;
        }
        
        .traffic-light .light.green.active {
          background: #22c55e;
          box-shadow: 0 0 16px #22c55e, inset 0 0 8px rgba(255, 255, 255, 0.4);
        }
        
        /* Boat Traffic Light (2 lights: Red, Green) */
        .boat-traffic-light {
          display: flex;
          flex-direction: column;
          align-items: center;
          gap: 6px;
          padding: 9px;
          background: #1f2937;
          border-radius: 12px;
          box-shadow: 0 3px 9px rgba(0, 0, 0, 0.3);
        }
        
        .boat-traffic-light .light {
          width: 27px;
          height: 27px;
          border-radius: 50%;
          background: #374151;
          border: 3px solid #111827;
          transition: background-color 0.3s ease, box-shadow 0.3s ease, opacity 0.3s ease;
          opacity: 0.3;
        }
        
        .boat-traffic-light .light.active {
          opacity: 1;
          box-shadow: 0 0 12px currentColor, inset 0 0 6px rgba(255, 255, 255, 0.3);
        }
        
        .boat-traffic-light .light.red {
          background: #dc2626;
        }
        
        .boat-traffic-light .light.red.active {
          background: #ef4444;
          box-shadow: 0 0 18px #ef4444, inset 0 0 8px rgba(255, 255, 255, 0.4);
        }
        
        .boat-traffic-light .light.green {
          background: #16a34a;
        }
        
        .boat-traffic-light .light.green.active {
          background: #22c55e;
          box-shadow: 0 0 18px #22c55e, inset 0 0 8px rgba(255, 255, 255, 0.4);
        }
      `}</style>
    </div>
  );
}
