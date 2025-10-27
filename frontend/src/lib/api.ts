import { ESPWebSocketClient } from "./wsClient";
import type {
  BridgeStatus,
  CarTrafficStatus,
  BoatTrafficStatus,
  SystemStatus,
  ResetResponse,
  VehicleTrafficStatus,
} from "./schema";

let client: ESPWebSocketClient | null = null;

export interface ConsoleCommandResponse {
  command: string;
  handled: boolean;
}

export function getESPClient(url?: string | null) {
  if (!client && url) {
    client = new ESPWebSocketClient(url);
    client.connect();
  }
  if (!client) {
    throw new Error("Client failed to be initialised.");
  }
  return client;
}

export const getBridgeState = () => getESPClient().request<BridgeStatus>("GET", "/bridge/status");

export const getCarTrafficState = () =>
  getESPClient().request<CarTrafficStatus>("GET", "/traffic/car/status");

export const getBoatTrafficState = () =>
  getESPClient().request<BoatTrafficStatus>("GET", "/traffic/boat/status");

export const getVehicleTrafficStatus = () =>
  getESPClient().request<VehicleTrafficStatus>("GET", "/traffic/vehicles/status");

export const getSystemState = () => getESPClient().request<SystemStatus>("GET", "/system/status");

export const setBridgeState = (state: "Open" | "Closed") =>
  getESPClient().request<BridgeStatus, { state: "Open" | "Closed" }>("SET", "/bridge/state", {
    state,
  });

// Car traffic control
export const setCarTrafficState = (value: "Red" | "Yellow" | "Green") =>
  getESPClient().request<CarTrafficStatus, { value: "Red" | "Yellow" | "Green" }>(
    "SET",
    "/traffic/car",
    { value }
  );

export const setBoatTrafficState = (side: "left" | "right", value: "Red" | "Green") =>
  getESPClient().request<BoatTrafficStatus, { side: "left" | "right"; value: "Red" | "Green" }>(
    "SET",
    "/traffic/boat/light",
    { side, value }
  );

export const resetSystem = () =>
  getESPClient().request<ResetResponse, Record<string, never>>("SET", "/system/reset", {});

export const sendConsoleCommand = (command: string) =>
  getESPClient().request<ConsoleCommandResponse, { command: string }>("SET", "/console/command", {
    command,
  });
