export enum IP {
  JOSH_1 = "ws://192.168.0.173/ws",
  AARON_1 = "ws://192.168.1.247/ws",
  JOSH_2 = "ws://172.20.10.7/ws",
  AARON_2 = "ws://172.20.10.13/ws",
  AARON_3 = "ws://172.20.10.2/ws",
  AARON_4 = "ws://192.168.1.236/ws",
}

export type ActivityEntry = {
  type: "sent" | "received";
  message: string;
  timestamp: number;
};

export enum Icon {
  BRIDGE = "BRIDGE",
  CAR = "CAR",
  BOAT = "BOAT",
  SYSTEM = "SYSTEM",
  PACKETS_SEND = "PACKETS_SEND",
  PACKETS_REC = "PACKETS_REC",
  ACTIVITY = "ACTIVITY",
}
