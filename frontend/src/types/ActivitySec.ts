import { ActivityEntry } from "./GenTypes";
import { BridgeState } from "../lib/schema";

export type ActivitySecProps = {
  log: ActivityEntry[];
  bridgeState?: BridgeState;
};
