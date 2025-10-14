import { CarTrafficState, BoatTrafficState, SystemState, BridgeState } from "../lib/schema";
import { DropdownOption } from "./CustomDropdown";
import { Icon } from "./GenTypes";

export type Status =
  | { kind: "car"; value: CarTrafficState }
  | { kind: "boat"; value: BoatTrafficState }
  | { kind: "system"; value: SystemState }
  | { kind: "bridge"; value: BridgeState };

type BaseProps = {
  title: string;
  description: string;
  updatedAt?: string;
  iconT?: Icon;
};

type StateCardProps = BaseProps & {
  variant: "STATE";
  options: DropdownOption[];
  status?: Status;
};

type DualStateCardProps = {
  title: string;
  variant: "DUAL_STATE";
  iconT?: Icon;
  leftLabel: string;
  leftOptions: DropdownOption[];
  leftStatus?: Status;
  rightLabel: string;
  rightOptions: DropdownOption[];
  rightStatus?: Status;
  updatedAt?: string;
};

type MetricCardProps = BaseProps & {
  variant?: "METRIC";
  options?: never;
  status?: never;
};

export type DashCardProp = StateCardProps | DualStateCardProps | MetricCardProps;
