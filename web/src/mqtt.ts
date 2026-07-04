// Thin wrapper over mqtt.js for publishing drone control commands over
// WebSocket (WSS). GitHub Pages is served over HTTPS, and browsers can only
// open a WebSocket to the drone via a public MQTT broker's WSS endpoint
// (e.g. wss://broker.hivemq.com:8884/mqtt) — direct UDP/ws:// is not possible.

import mqtt, { type MqttClient } from "mqtt";

export type MqttStatus = "disconnected" | "connecting" | "connected" | "error";
export type MqttStatusHandler = (status: MqttStatus, detail: string) => void;

export const DEFAULT_BROKER = "wss://broker.hivemq.com:8884/mqtt";

export class MqttPublisher {
  private client: MqttClient | null = null;

  constructor(private onStatus: MqttStatusHandler) {}

  connect(url: string): void {
    this.disconnect();
    this.onStatus("connecting", `接続中… ${url}`);
    const client = mqtt.connect(url, {
      connectTimeout: 8000,
      reconnectPeriod: 3000,
      clean: true,
    });
    this.client = client;
    client.on("connect", () => this.onStatus("connected", `接続済み ${url}`));
    client.on("reconnect", () => this.onStatus("connecting", "再接続中…"));
    client.on("error", (err: Error) => this.onStatus("error", `エラー: ${err.message}`));
    client.on("close", () => this.onStatus("disconnected", "切断"));
  }

  get connected(): boolean {
    return this.client?.connected ?? false;
  }

  publish(topic: string, payload: string): void {
    if (this.client?.connected) this.client.publish(topic, payload);
  }

  disconnect(): void {
    this.client?.end(true);
    this.client = null;
  }
}
