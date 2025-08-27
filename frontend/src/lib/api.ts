import { ESPWebSocketClient } from "./wsClient";
import type { BridgeStatus, CarTrafficStatus, BoatTrafficStatus, SystemStatus } from "./schema";

let client: ESPWebSocketClient | null = null;

export function getESPClient(url = "ws://192.168.0.173/ws") {
  // export function getESPClient(url = "ws://172.20.10.7/ws") {
  if (!client) {
    client = new ESPWebSocketClient(url);
    client.connect();
  }
  return client;
}

export const getBridgeStatus = () => getESPClient().request<BridgeStatus>("GET", "/bridge/status");

export const getCarTrafficStatus = () =>
  getESPClient().request<CarTrafficStatus>("GET", "/traffic/car/status");

export const getBoatTrafficStatus = () =>
  getESPClient().request<BoatTrafficStatus>("GET", "/traffic/boat/status");

export const getSystemStatus = () => getESPClient().request<SystemStatus>("GET", "/system/status");

export const setBridgeState = (state: "Open" | "Closed") =>
  getESPClient().request<BridgeStatus, { state: "Open" | "Closed" }>("SET", "/bridge/state", {
    state,
  });

export const setCarTraffic = (side: "left" | "right", value: "Red" | "Yellow" | "Green") =>
  getESPClient().request<CarTrafficStatus, { side: "left" | "right"; value: "Red" | "Yellow" | "Green" }>(
    "SET",
    "/traffic/car/light",
    { side, value }
  );

export const setBoatTraffic = (side: "left" | "right", value: "Red" | "Green") =>
  getESPClient().request<BoatTrafficStatus, { side: "left" | "right"; value: "Red" | "Green" }>(
    "SET",
    "/traffic/boat/light",
    { side, value }
  );
