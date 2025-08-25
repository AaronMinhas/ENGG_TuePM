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

export type BridgeState = "OPENING" | "CLOSING" | "OPEN" | "CLOSED" | "ERROR";
export interface BridgeStatus {
  state: BridgeState;
  lastChangeMs: number;
  lockEngaged: boolean;
}

export type TrafficLight = "RED" | "YELLOW" | "GREEN";
export interface TrafficStatus {
  left: TrafficLight;
  right: TrafficLight;
}

export interface SystemStatus {
  connection: "CONNECTED" | "CONNECTING" | "DISCONNECTED";
  rssi?: number;
  uptimeMs?: number;
}
