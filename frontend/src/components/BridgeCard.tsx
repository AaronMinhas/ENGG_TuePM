/**
 * This component will be in the centre of the dashboard.
 * Its purpose is to display the bridges current state in an interesting way for the user, such as
 * showing a "bridge" open or closed based on the state.
 */

import React from "react";
import type { BridgeStatus } from "../lib/schema";

interface BridgeCardProps {
  bridgeStatus?: BridgeStatus | null;
}

export default function BridgeCard({ bridgeStatus }: BridgeCardProps) {
  const bridgeState = bridgeStatus?.state || "Closed";
  const lockEngaged = bridgeStatus?.lockEngaged || false;
  
  const getBridgeClasses = () => {
    switch (bridgeState) {
      case "Open":
        return "bridge-open";
      case "Opening":
        return "bridge-opening";
      case "Closing":
        return "bridge-closing";
      case "Error":
        return "bridge-error";
      default:
        return "bridge-closed";
    }
  };

  const getStatusText = () => {
    switch (bridgeState) {
      case "Opening":
        return "Opening...";
      case "Closing":
        return "Closing...";
      case "Error":
        return "Error";
      default:
        return bridgeState;
    }
  };

  const getStatusColor = () => {
    switch (bridgeState) {
      case "Open":
        return "text-green-600";
      case "Opening":
      case "Closing":
        return "text-yellow-600";
      case "Error":
        return "text-red-600";
      default:
        return "text-gray-600";
    }
  };

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
          {bridgeState === "Error" && (
            <div className="error-indicator">
              <div className="error-icon">!</div>
            </div>
          )}
          
        </div>
      </div>
      
      {/* Status Text */}
      <div className="text-center mt-2">
        <div className={`text-lg font-semibold ${getStatusColor()}`}>
          {getStatusText()}
        </div>
        {bridgeStatus?.lastChangeMs && (
          <div className="text-xs text-gray-500 mt-1">
            Last changed: {new Date(bridgeStatus.lastChangeMs).toLocaleTimeString()}
          </div>
        )}
      </div>
      
      {/* Custom CSS for animations */}
      <style>{`
        .bridge-container {
          width: 400px;
          height: 180px;
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
          height: 40px;
          background: linear-gradient(to bottom, #3b82f6, #1e40af);
          border-radius: 0 0 8px 8px;
        }
        
        .bridge {
          position: relative;
          width: 300px;
          height: 80px;
          display: flex;
          align-items: center;
          justify-content: center;
          transition: all 0.8s cubic-bezier(0.4, 0, 0.2, 1);
        }
        
        /* Bridge Supports */
        .center-support {
          position: absolute;
          bottom: -15px;
          width: 12px;
          height: 30px;
          background: #374151;
          border-radius: 2px;
          z-index: 2;
          box-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);
        }
        
        .left-support {
          left: 50px;
          transform: translateX(-50%);
        }
        
        .right-support {
          right: 50px;
          transform: translateX(50%);
        }
        
        .center-support::before {
          content: '';
          position: absolute;
          top: -8px;
          left: -3px;
          right: -3px;
          height: 8px;
          background: #374151;
          border-radius: 2px;
        }
        
        /* Stationary Road Sections */
        .stationary-road {
          position: absolute;
          height: 20px;
          background: linear-gradient(to bottom, #6b7280, #4b5563);
          border-radius: 4px;
          box-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);
          z-index: 1;
        }
        
        .stationary-road::before {
          content: '';
          position: absolute;
          top: 2px;
          left: 2px;
          right: 2px;
          height: 2px;
          background: #9ca3af;
        }
        
        .left-road {
          left: 20px;
          top: 40px;
          width: 70px;
        }
        
        .right-road {
          right: 20px;
          top: 40px;
          width: 70px;
        }
        
        /* Rotating Bridge Halves */
        .bridge-half {
          position: absolute;
          width: 60px;
          height: 60px;
          transition: all 0.8s cubic-bezier(0.4, 0, 0.2, 1);
        }
        
        .left-half {
          left: 90px;
          top: 40px;
          transform-origin: left;
        }
        
        .right-half {
          right: 90px;
          top: 40px;
          transform-origin: right;
        }
        
        .bridge-deck {
          width: 100%;
          height: 20px;
          background: linear-gradient(to bottom, #6b7280, #4b5563);
          border-radius: 4px;
          position: relative;
          box-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);
        }
        
        .bridge-deck::before {
          content: '';
          position: absolute;
          top: 2px;
          left: 2px;
          right: 2px;
          height: 2px;
          background: #9ca3af;
        }
        
        /* Bridge States */
        .bridge-closed .left-half {
          transform: rotate(0deg);
        }
        
        .bridge-closed .right-half {
          transform: rotate(0deg);
        }
        
        .bridge-opening .left-half {
          animation: openLeft 2s ease-in-out forwards;
        }
        
        .bridge-opening .right-half {
          animation: openRight 2s ease-in-out forwards;
        }
        
        .bridge-open .left-half {
          transform: rotate(-45deg);
        }
        
        .bridge-open .right-half {
          transform: rotate(45deg);
        }
        
        .bridge-closing .left-half {
          animation: closeLeft 2s ease-in-out forwards;
        }
        
        .bridge-closing .right-half {
          animation: closeRight 2s ease-in-out forwards;
        }
        
        .bridge-error {
          animation: errorShake 0.5s ease-in-out infinite;
        }
        
        /* Animations */
        @keyframes openLeft {
          0% { transform: rotate(0deg); }
          100% { transform: rotate(-45deg); }
        }
        
        @keyframes openRight {
          0% { transform: rotate(0deg); }
          100% { transform: rotate(45deg); }
        }
        
        @keyframes closeLeft {
          0% { transform: rotate(-45deg); }
          100% { transform: rotate(0deg); }
        }
        
        @keyframes closeRight {
          0% { transform: rotate(45deg); }
          100% { transform: rotate(0deg); }
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
      `}</style>
    </div>
  );
}
