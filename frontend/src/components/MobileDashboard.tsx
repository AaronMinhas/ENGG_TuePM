import React from "react";
import { BridgeStatus, CarTrafficStatus, BoatTrafficStatus, SystemStatus, CarTrafficState, BoatTrafficState } from "../lib/schema";
import { Icon, ActivityEntry } from "../types/GenTypes";
import { timeAgo } from "../utils/timeAgo";
import DashCard from "./DashCard";
import BridgeCard from "./BridgeCard";
import ActivitySec from "./ActivitySec";

interface MobileDashboardProps {
  readonly bridgeStatus: BridgeStatus | null;
  readonly carTrafficStatus: CarTrafficStatus | null;
  readonly boatTrafficStatus: BoatTrafficStatus | null;
  readonly systemStatus: SystemStatus | null;
  readonly packetsSent: number;
  readonly packetsReceived: number;
  readonly lastSentAt: number | null;
  readonly lastReceivedAt: number | null;
  readonly activityLog: ActivityEntry[];
  readonly handleOpenBridge: () => void;
  readonly handleCloseBridge: () => void;
  readonly handleCarTraffic: (value: CarTrafficState) => void;
  readonly handleBoatTraffic: (side: "left" | "right", value: BoatTrafficState) => void;
  readonly handleFetchSystem: () => void;
}

/**
 * Mobile dashboard layout component (2x8 grid)
 */
export default function MobileDashboard({
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
}: MobileDashboardProps) {
  return (
    <div className="grid lg:hidden grid-cols-2 grid-rows-8 h-[calc(100dvh - 300px)] min-h-0 w-full gap-4 mx-4 mb-4">
      {/* Activity - 2x2 (spans rows 1-2, cols 1-2) */}
      <div className="col-span-2 row-span-2">
        <ActivitySec log={activityLog} />
      </div>

      {/* Bridge - 2x2 (spans rows 3-4, cols 1-2) */}
      <div className="col-span-2 row-span-2">
        <BridgeCard bridgeStatus={bridgeStatus} />
      </div>

      {/* DashCards - 1x1 each in 2x4 grid (rows 5-8) */}
      <DashCard
        title="Bridge State"
        variant="STATE"
        iconT={Icon.BRIDGE}
        options={[
          { id: "m-b-o", label: "Open", action: handleOpenBridge },
          { id: "m-b-c", label: "Close", action: handleCloseBridge },
        ]}
        description={bridgeStatus?.state || ""}
        updatedAt={bridgeStatus?.receivedAt ? timeAgo(bridgeStatus?.receivedAt) : ""}
        status={bridgeStatus?.state ? { kind: "bridge", value: bridgeStatus.state } : undefined}
      />
      <DashCard
        title="System State"
        variant="STATE"
        iconT={Icon.SYSTEM}
        options={[{ id: "m-s", label: "Update Status", action: handleFetchSystem }]}
        description={systemStatus?.connection || ""}
        updatedAt={systemStatus?.receivedAt ? timeAgo(systemStatus?.receivedAt) : ""}
        status={systemStatus?.connection ? { kind: "system", value: systemStatus.connection } : undefined}
      />
      <DashCard
        title="Car Traffic"
        variant="STATE"
        iconT={Icon.CAR}
        options={[
          { id: "m-c-r", label: "Red", action: () => handleCarTraffic("Red") },
          { id: "m-c-y", label: "Yellow", action: () => handleCarTraffic("Yellow") },
          { id: "m-c-g", label: "Green", action: () => handleCarTraffic("Green") },
        ]}
        description={`L:${carTrafficStatus?.left.value || "?"} R:${carTrafficStatus?.right.value || "?"}`}
        updatedAt={carTrafficStatus?.left.receivedAt ? timeAgo(carTrafficStatus?.left.receivedAt) : ""}
        status={
          carTrafficStatus?.left.value ? { kind: "car", value: carTrafficStatus.left.value } : undefined
        }
      />
      <DashCard
        title="Boat Traffic"
        variant="STATE"
        iconT={Icon.BOAT}
        options={[
          { id: "m-bt-r", label: "Red", action: () => handleBoatTraffic("left", "Red") },
          { id: "m-bt-g", label: "Green", action: () => handleBoatTraffic("left", "Green") },
        ]}
        description={`L:${boatTrafficStatus?.left.value || "?"} R:${boatTrafficStatus?.right.value || "?"}`}
        updatedAt={boatTrafficStatus?.left.receivedAt ? timeAgo(boatTrafficStatus?.left.receivedAt) : ""}
        status={
          boatTrafficStatus?.left.value
            ? { kind: "boat", value: boatTrafficStatus.left.value }
            : undefined
        }
      />
      <DashCard
        title="Packets Sent"
        iconT={Icon.PACKETS_SEND}
        description={packetsSent.toString()}
        updatedAt={lastSentAt ? timeAgo(lastSentAt) : ""}
      />
      <DashCard
        title="Packets Received"
        iconT={Icon.PACKETS_REC}
        description={packetsReceived.toString()}
        updatedAt={lastReceivedAt ? timeAgo(lastReceivedAt) : ""}
      />
    </div>
  );
}

