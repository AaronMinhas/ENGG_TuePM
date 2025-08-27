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

export const AnyInbound = ResponseMsg;
export type RequestMsgT = z.infer<typeof RequestMsg>;
export type ResponseMsgT = z.infer<typeof ResponseMsg>;

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

export type CarTrafficLight = "Red" | "Yellow" | "Green";
export interface CarTrafficStatus {
  left: TrafficLightStatus<CarTrafficLight>;
  right: TrafficLightStatus<CarTrafficLight>;
}

export type BoatTrafficLight = "Red" | "Green";
export interface BoatTrafficStatus {
  left: TrafficLightStatus<BoatTrafficLight>;
  right: TrafficLightStatus<BoatTrafficLight>;
}

export interface SystemStatus {
  connection: "Connected" | "Connecting" | "Disconnected";
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
