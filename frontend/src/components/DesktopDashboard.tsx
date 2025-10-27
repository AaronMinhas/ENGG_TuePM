import React from "react";
import {
  BridgeStatus,
  CarTrafficStatus,
  BoatTrafficStatus,
  SystemStatus,
  CarTrafficState,
  BoatTrafficState,
  VehicleTrafficStatus,
} from "../lib/schema";
import { Icon, ActivityEntry } from "../types/GenTypes";
import { timeAgo } from "../utils/timeAgo";
import DashCard from "./DashCard";
import BridgeCard from "./BridgeCard";
import ActivitySec from "./ActivitySec";
import { VehicleQueueCard } from "./VehicleQueueCard";

interface DesktopDashboardProps {
  readonly bridgeStatus: BridgeStatus | null;
  readonly carTrafficStatus: CarTrafficStatus | null;
  readonly boatTrafficStatus: BoatTrafficStatus | null;
  readonly vehicleTrafficStatus: VehicleTrafficStatus | null;
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
  readonly controlsDisabled?: boolean;
}

export default function DesktopDashboard({
  bridgeStatus,
  carTrafficStatus,
  boatTrafficStatus,
  vehicleTrafficStatus,
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
  controlsDisabled = false,
}: DesktopDashboardProps) {
  return (
    <div className="hidden lg:grid grid-cols-4 grid-rows-4 gap-4 lg:h-[calc(100vh-300px)] max-w-[1700px] w-full mx-4">
      <DashCard
        title="Bridge State"
        variant="STATE"
        iconT={Icon.BRIDGE}
        options={[
          { id: "d-b-o", label: "Open", action: handleOpenBridge },
          { id: "d-b-c", label: "Close", action: handleCloseBridge },
        ]}
        description={bridgeStatus?.state || ""}
        updatedAt={bridgeStatus?.receivedAt ? timeAgo(bridgeStatus?.receivedAt) : ""}
        status={bridgeStatus?.state ? { kind: "bridge", value: bridgeStatus.state } : undefined}
        disabled={controlsDisabled}
      />
      <DashCard
        title="Car Traffic"
        variant="STATE"
        iconT={Icon.CAR}
        options={[
          { id: "d-c-r", label: "Red", action: () => handleCarTraffic("Red") },
          { id: "d-c-y", label: "Yellow", action: () => handleCarTraffic("Yellow") },
          { id: "d-c-g", label: "Green", action: () => handleCarTraffic("Green") },
        ]}
        description={`L:${carTrafficStatus?.left.value || "?"} R:${carTrafficStatus?.right.value || "?"}`}
        updatedAt={carTrafficStatus?.left.receivedAt ? timeAgo(carTrafficStatus?.left.receivedAt) : ""}
        status={
          carTrafficStatus?.left.value ? { kind: "car", value: carTrafficStatus.left.value } : undefined
        }
        disabled={controlsDisabled}
      />
      <DashCard
        title="Boat Traffic"
        variant="DUAL_STATE"
        iconT={Icon.BOAT}
        leftLabel="Left"
        leftOptions={[
          { id: "d-bt-l-r", label: "Red", action: () => handleBoatTraffic("left", "Red") },
          { id: "d-bt-l-g", label: "Green", action: () => handleBoatTraffic("left", "Green") },
        ]}
        leftStatus={
          boatTrafficStatus?.left.value
            ? { kind: "boat", value: boatTrafficStatus.left.value }
            : undefined
        }
        rightLabel="Right"
        rightOptions={[
          { id: "d-bt-r-r", label: "Red", action: () => handleBoatTraffic("right", "Red") },
          { id: "d-bt-r-g", label: "Green", action: () => handleBoatTraffic("right", "Green") },
        ]}
        rightStatus={
          boatTrafficStatus?.right.value
            ? { kind: "boat", value: boatTrafficStatus.right.value }
            : undefined
        }
        updatedAt={boatTrafficStatus?.left.receivedAt ? timeAgo(boatTrafficStatus?.left.receivedAt) : ""}
        disabled={controlsDisabled}
      />
      <VehicleQueueCard data={vehicleTrafficStatus} />
      <div className="row-span-4">
        <ActivitySec log={activityLog} />
      </div>

      <DashCard
        title="System State"
        variant="STATE"
        iconT={Icon.SYSTEM}
        options={[{ id: "d-s", label: "Update Status", action: handleFetchSystem }]}
        description={systemStatus?.connection || ""}
        updatedAt={systemStatus?.receivedAt ? timeAgo(systemStatus?.receivedAt) : ""}
        status={systemStatus?.connection ? { kind: "system", value: systemStatus.connection } : undefined}
      />
      <div className="col-span-2 row-span-3">
        <BridgeCard bridgeStatus={bridgeStatus} />
      </div>

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
