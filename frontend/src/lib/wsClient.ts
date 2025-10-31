import { nanoid } from "nanoid";
import { AnyInbound, ResponseMsg, RequestMsgT, EventMsgT } from "./schema";

type Pending = {
  resolve: (val: unknown) => void;
  reject: (err: unknown) => void;
  timer: ReturnType<typeof setTimeout>;
};

export class ESPWebSocketClient {
  private url: string;
  private ws: WebSocket | null = null;
  private pending = new Map<string, Pending>();
  private reconnectAttempts = 0;
  private heartbeatTimer: ReturnType<typeof setTimeout> | null = null;
  private lastPong = Date.now();
  private statusListener?: (s: "Open" | "Closed" | "Connecting") => void;
  private eventListener?: (e: EventMsgT) => void;

  constructor(url: string) {
    this.url = url;
  }

  onStatus(listener: (s: "Open" | "Closed" | "Connecting") => void) {
    this.statusListener = listener;
  }

  onEvent(listener: (e: EventMsgT) => void) {
    this.eventListener = listener;
  }

  connect() {
    if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING))
      return;

    this.notifyStatus("Connecting");
    this.ws = new WebSocket(this.url);

    this.ws.onopen = () => {
      this.reconnectAttempts = 0;
      this.setupHeartbeat();
      this.notifyStatus("Open");
    };

    this.ws.onmessage = (ev) => {
      try {
        const anyMsg = AnyInbound.parse(JSON.parse(ev.data));
        if ((anyMsg as any).type === "response") {
          const msg = ResponseMsg.parse(anyMsg);
          const p = this.pending.get(msg.id);
          if (p) {
            clearTimeout(p.timer);
            this.pending.delete(msg.id);
            msg.ok ? p.resolve(msg.payload) : p.reject(new Error(msg.error || "ESP error"));
          }
          return;
        }
        // Event message
        const evt = anyMsg as EventMsgT;
        this.eventListener?.(evt);
      } catch {
        // Ignore
      }
    };

    this.ws.onclose = () => {
      this.notifyStatus("Closed");
      this.clearHeartbeat();
      for (const [id, p] of this.pending) {
        clearTimeout(p.timer);
        p.reject(new Error("Socket closed"));
        this.pending.delete(id);
      }
      this.scheduleReconnect();
    };

    this.ws.onerror = () => {};
  }

  close() {
    this.clearHeartbeat();
    this.ws?.close();
    this.ws = null;
  }

  reconnect() {
    console.log("[WS] Manual reconnect requested");
    this.close();
    this.reconnectAttempts = 0; // Reset attempts for manual reconnect
    // Small delay to ensure close completes
    setTimeout(() => {
      this.connect();
    }, 100);
  }

  private notifyStatus(s: "Open" | "Closed" | "Connecting") {
    this.statusListener?.(s);
  }

  private scheduleReconnect() {
    const delay = Math.min(30000, 500 * 2 ** this.reconnectAttempts++);
    setTimeout(() => this.connect(), delay);
  }

  private setupHeartbeat() {
    this.clearHeartbeat();
    this.lastPong = Date.now();
    this.heartbeatTimer = setInterval(() => {
      if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return;
      const id = nanoid();
      this.sendRaw({ v: 1, id, type: "request", method: "GET", path: "/system/ping" }, 4000)
        .then(() => (this.lastPong = Date.now()))
        .catch(() => {
          this.ws?.close();
        });
    }, 10000);
  }

  private clearHeartbeat() {
    if (this.heartbeatTimer) clearInterval(this.heartbeatTimer);
    this.heartbeatTimer = null;
  }

  private sendRaw(
    msg: Omit<RequestMsgT, "payload"> & { payload?: unknown },
    timeoutMs = 8000
  ): Promise<unknown> {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      return Promise.reject(new Error("Socket not open"));
    }
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(msg.id);
        reject(new Error(`Timeout waiting for ${msg.path}`));
      }, timeoutMs);
      this.pending.set(msg.id, { resolve, reject, timer });
      // this.ws!.send(JSON.stringify(msg));
      const out = JSON.stringify(msg);
      // console.log("[WS][TX]", out);
      this.ws!.send(out);
    });
  }

  request<TResp = unknown, TReq = unknown>(
    method: "GET" | "SET",
    path: string,
    payload?: TReq,
    timeoutMs?: number
  ) {
    const id = nanoid();
    return this.sendRaw({ v: 1, id, type: "request", method, path, payload }, timeoutMs) as Promise<TResp>;
  }
}
