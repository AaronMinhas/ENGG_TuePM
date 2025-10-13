import React from "react";
import TopNav from "./components/TopNav";
import DesktopDashboard from "./components/DesktopDashboard";
import MobileDashboard from "./components/MobileDashboard";
import { usePacketTracking } from "./hooks/usePacketTracking";
import { useActivityLog } from "./hooks/useActivityLog";
import { useESPStatus } from "./hooks/useESPStatus";
import { useESPWebSocket } from "./hooks/useESPWebSocket";

function App() {
  const { packetsSent, packetsReceived, lastSentAt, lastReceivedAt, incrementSent, incrementReceived } =
    usePacketTracking();

  const { activityLog, logActivity } = useActivityLog();

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

  useESPWebSocket({
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
