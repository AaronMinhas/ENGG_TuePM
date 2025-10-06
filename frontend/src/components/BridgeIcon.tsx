import { createLucideIcon } from "lucide-react";

const BridgeIcon = createLucideIcon("Bridge", [
  ["line", { x1: "2", y1: "18", x2: "22", y2: "18" }],
  ["line", { x1: "6", y1: "18", x2: "6", y2: "14" }],
  ["line", { x1: "18", y1: "18", x2: "18", y2: "14" }],
  ["path", { d: "M6 14c3-4 9-4 12 0" }],
  ["line", { x1: "6", y1: "14", x2: "18", y2: "14" }],
]);

export default BridgeIcon;
