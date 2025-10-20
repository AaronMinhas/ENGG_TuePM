import { z } from "zod";

export const WsMsgBase = z.object({
  v: z.literal(1),
  id: z.string().min(1),
  ts: z.number().optional(),
});

export const RequestMsg = WsMsgBase.extend({
  type: z.literal("request"),
  method: z.enum(["GET", "SET"]),
  path: z.string().startsWith("/"),
  payload: z.unknown().optional(),
});

export const ResponseMsg = WsMsgBase.extend({
  type: z.literal("response"),
  ok: z.boolean(),
  path: z.string().startsWith("/"),
  payload: z.unknown().optional(),
  error: z.string().optional(),
});

export const EventMsg = z.object({
  v: z.literal(1),
  type: z.literal("event"),
  path: z.string().startsWith("/"),
  payload: z.unknown().optional(),
});

export const AnyInbound = z.union([ResponseMsg, EventMsg]);
export type RequestMsgT = z.infer<typeof RequestMsg>;
export type ResponseMsgT = z.infer<typeof ResponseMsg>;
export type EventMsgT = z.infer<typeof EventMsg>;

export type BridgeState = "Opening" | "Closing" | "Open" | "Closed" | "Error";
export interface BridgeStatus {
  state: BridgeState;
  lastChangeMs: number;
  lockEngaged: boolean;
  receivedAt?: number;
}

export interface TrafficLightStatus<TLight> {
  value: TLight;
  receivedAt?: number;
}

export type CarTrafficState = "Red" | "Yellow" | "Green";
export interface CarTrafficStatus {
  left: TrafficLightStatus<CarTrafficState>;
  right: TrafficLightStatus<CarTrafficState>;
}

export type BoatTrafficState = "Red" | "Green";
export interface BoatTrafficStatus {
  left: TrafficLightStatus<BoatTrafficState>;
  right: TrafficLightStatus<BoatTrafficState>;
}

export type SystemState = "Connected" | "Connecting" | "Disconnected";
export interface SystemStatus {
  connection: SystemState;
  rssi?: number;
  uptimeMs?: number;
  receivedAt?: number;
}

export interface Timed<T> {
  receivedAt: number;
  data: T;
}

export type TimedCarTrafficStatus = Timed<CarTrafficStatus>;
export type TimedBridgeStatus = Timed<BridgeStatus>;
