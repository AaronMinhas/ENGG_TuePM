/**
 * What needs to be done here:
 * - Need to add an indication of WIFI strength --> Using RSSI range -- closer to 0 the stronger
 * - Need to wire in dropdown buttons that fetch each relevant status of the system
 * - Activity log in UI
 */

import { useEffect, useState } from "react";
import {
  getESPClient,
  getBridgeStatus,
  getTrafficStatus,
  getSystemStatus,
  setBridgeState,
  setTraffic,
} from "./lib/api";
import type { BridgeStatus, TrafficStatus, SystemStatus } from "./lib/schema";

import TopNav from "./components/TopNav";
import DashCard from "./components/DashCard";
import BridgeCard from "./components/BridgeCard";
import ActivitySec from "./components/ActivitySec";

function App() {
  const [wsStatus, setWsStatus] = useState<"CONNECTING" | "OPEN" | "CLOSED">("CONNECTING");
  const [bridge, setBridge] = useState<BridgeStatus | null>(null);
  const [traffic, setTrafficState] = useState<TrafficStatus | null>(null);
  const [system, setSystem] = useState<SystemStatus | null>(null);

  const handleFetchBridge = async () => {
    try {
      const data = await getBridgeStatus();
      setBridge(data);
    } catch (e) {
      console.error("Bridge status error:", e);
    }
  };

  const handleFetchTraffic = async () => {
    try {
      const data = await getTrafficStatus();
      setTrafficState(data);
    } catch (e) {
      console.error("Traffic status error:", e);
    }
  };

  const handleFetchSystem = async () => {
    try {
      const data = await getSystemStatus();
      setSystem(data);
    } catch (e) {
      console.error("System status error:", e);
    }
  };

  const handleOpenBridge = async () => {
    const data = await setBridgeState("OPEN");
    setBridge(data);
  };

  const handleCloseBridge = async () => {
    const data = await setBridgeState("CLOSED");
    setBridge(data);
  };

  const handleTraffic = async (side: "left" | "right", color: "RED" | "YELLOW" | "GREEN") => {
    const data = await setTraffic(side, color);
    setTrafficState(data);
  };

  useEffect(() => {
    const client = getESPClient("ws://192.168.0.173/ws");
    client.onStatus(setWsStatus);

    const poll = async () => {
      try {
        const [b, t, s] = await Promise.all([getBridgeStatus(), getTrafficStatus(), getSystemStatus()]);
        setBridge(b);
        setTrafficState(t);
        setSystem(s);
      } catch (err) {
        console.error("Poll error:", err);
      }
    };

    poll();
    const id = setInterval(poll, 5000);

    return () => clearInterval(id);
  }, []);

  return (
    <div className="bg-gray-100 min-h-screen w-full">
      <TopNav />

      <div>
        <p>
          <strong>WebSocket:</strong> {wsStatus}
        </p>
        <button className="px-3 py-1 m-1 bg-gray-200 rounded" onClick={handleFetchSystem}>
          Get System Status
        </button>
        {system && <pre className="bg-gray-100 p-2 rounded mt-2">{JSON.stringify(system, null, 2)}</pre>}
      </div>

      <p>
        <strong>Bridge State:</strong> {bridge?.state ?? "?"}
      </p>
      <button className="px-3 py-1 m-1 bg-green-200 rounded" onClick={handleOpenBridge}>
        Open Bridge
      </button>
      <button className="px-3 py-1 m-1 bg-red-200 rounded" onClick={handleCloseBridge}>
        Close Bridge
      </button>
      <button className="px-3 py-1 m-1 bg-gray-200 rounded" onClick={handleFetchBridge}>
        Get Bridge Status
      </button>
      {bridge && <pre className="bg-gray-100 p-2 rounded mt-2">{JSON.stringify(bridge, null, 2)}</pre>}

      <div className="flex justify-center">
        <div className="grid grid-cols-4 grid-rows-4 gap-4 max-w-[1700px] w-full mx-4 mt-4">
          <DashCard
            title="Bridge State"
            description="Closed"
            updatedAt="3 minutes ago"
            cardType="STATE"
            iconType="BRIDGE"
            bridgeStateType="CLOSED"
          />
          <DashCard
            title="Car Traffic"
            description="Green"
            updatedAt="3 minutes ago"
            cardType="STATE"
            iconType="CAR"
            carStateType="GREEN"
          />
          <DashCard
            title="Boat Traffic"
            description="Red"
            updatedAt="3 minutes ago"
            cardType="STATE"
            iconType="BOAT"
            boatStateType="RED"
          />
          <ActivitySec />

          <DashCard
            title="System State"
            description="Connected"
            updatedAt="3 minutes ago"
            cardType="STATE"
            iconType="SYSTEM"
            systemStateType="CONNECTED"
          />
          <BridgeCard />

          <DashCard
            title="Packets Sent"
            description="403"
            updatedAt="3 minutes ago"
            iconType="PACKETS_SEND"
          />
          <DashCard
            title="Packets Received"
            description="394"
            updatedAt="3 minutes ago"
            iconType="PACKETS_REC"
          />
        </div>
      </div>
    </div>
  );
}

export default App;
