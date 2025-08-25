import { ESPWebSocketClient } from "./wsClient";
import type { BridgeStatus, TrafficStatus, SystemStatus } from "./schema";

let client: ESPWebSocketClient | null = null;

export function getESPClient(url = "ws://192.168.0.173/ws") {
  if (!client) {
    client = new ESPWebSocketClient(url);
    client.connect();
  }
  return client;
}

export const getBridgeStatus = () => getESPClient().request<BridgeStatus>("GET", "/bridge/status");

export const getTrafficStatus = () => getESPClient().request<TrafficStatus>("GET", "/traffic/status");

export const getSystemStatus = () => getESPClient().request<SystemStatus>("GET", "/system/status");

export const setBridgeState = (state: "OPEN" | "CLOSED") =>
  getESPClient().request<BridgeStatus, { state: "OPEN" | "CLOSED" }>("SET", "/bridge/state", { state });

export const setTraffic = (side: "left" | "right", value: "RED" | "YELLOW" | "GREEN") =>
  getESPClient().request<TrafficStatus, { side: "left" | "right"; value: "RED" | "YELLOW" | "GREEN" }>(
    "SET",
    "/traffic/light",
    { side, value }
  );
