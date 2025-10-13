/**
 * Main dashboard that controls and monitors the ESP bridge system.
 *
 * Refactored into smaller, focused components and hooks:
 * - usePacketTracking: Manages packet counts with localStorage persistence
 * - useActivityLog: Manages activity log entries
 * - useESPStatus: Manages all ESP status states and control commands
 * - useESPWebSocket: Handles WebSocket connection and real-time updates
 * - DesktopDashboard: Desktop layout component
 * - MobileDashboard: Mobile layout component
 *
 * TODO:
 * - Persist Activity logs over refreshes
 * - Persist all states of the system over a refresh (packets already do this)
 * - Visual Bridge animation
 * - Need to add an indication of WIFI strength --> Using RSSI range -- closer to 0 the stronger
 */

import React from "react";
import TopNav from "./components/TopNav";
import DesktopDashboard from "./components/DesktopDashboard";
import MobileDashboard from "./components/MobileDashboard";
import { usePacketTracking } from "./hooks/usePacketTracking";
import { useActivityLog } from "./hooks/useActivityLog";
import { useESPStatus } from "./hooks/useESPStatus";
import { useESPWebSocket } from "./hooks/useESPWebSocket";

function App() {
  // Packet tracking with localStorage
  const { packetsSent, packetsReceived, lastSentAt, lastReceivedAt, incrementSent, incrementReceived } =
    usePacketTracking();

  // Activity log
  const { activityLog, logActivity } = useActivityLog();

  // ESP status and commands
  const {
    bridgeStatus,
    setBridgeStatus,
    carTrafficStatus,
    setCarTrafficStatus,
    boatTrafficStatus,
    setBoatTrafficStatus,
    systemStatus,
    setSystemStatus,
    handleFetchSystem,
    handleOpenBridge,
    handleCloseBridge,
    handleCarTraffic,
    handleBoatTraffic,
  } = useESPStatus(incrementSent, incrementReceived, logActivity);

  // WebSocket connection and real-time updates
  const { wsStatus } = useESPWebSocket({
    setBridgeStatus,
    setCarTrafficStatus,
    setBoatTrafficStatus,
    setSystemStatus,
    incrementReceived,
    logActivity,
    carTrafficStatus,
    boatTrafficStatus,
  });

  const dashboardProps = {
    bridgeStatus,
    carTrafficStatus,
    boatTrafficStatus,
    systemStatus,
    packetsSent,
    packetsReceived,
    lastSentAt,
    lastReceivedAt,
    activityLog,
    handleOpenBridge,
    handleCloseBridge,
    handleCarTraffic,
    handleBoatTraffic,
    handleFetchSystem,
  };

  return (
    <div className="bg-gray-100 min-h-screen w-full">
      <TopNav />
      <div className="flex justify-center mt-4">
        <DesktopDashboard {...dashboardProps} />
        <MobileDashboard {...dashboardProps} />
      </div>
    </div>
  );
}

export default App;
