// Maps mental state → a drone control command and throttles its emission.
//
// The command value is a float in [0, 10] — the exact range the StampFly
// firmware (src/eeg_mqtt_drone.cpp, mirroring getYawRateFromEEG) buckets into
// yaw + thrust. Keeping the same 0–10 contract lets the UI and drone agree.
//
// This module is deliberately transport-free (no MQTT import) so it stays a
// pure, unit-testable unit; main.ts wires the emitted payload to MqttPublisher.

export const CMD_MIN = 0.0;
export const CMD_MAX = 10.0;

// focus is the Pope engagement index β/(α+θ) from mind.ts, spanning roughly
// 0..2 in practice. Map linearly to 0..10 and clamp. Non-finite / ≤0 → 0.
export function focusToCommand(focus: number): number {
  if (!Number.isFinite(focus) || focus <= 0) return CMD_MIN;
  const v = (focus / 2) * CMD_MAX;
  return Math.max(CMD_MIN, Math.min(CMD_MAX, v));
}

export interface ControlCommand {
  v: number; // control value 0..10
  arm: boolean; // motors enabled only when true
  ts: number; // emitter timestamp (ms)
}

export interface DroneControllerOpts {
  // Called whenever a command should be sent on the wire (already serialized).
  publish: (payload: string) => void;
  // Minimum gap between publishes, ms. Default 100 (10 Hz).
  minIntervalMs?: number;
  // Injectable clock for deterministic tests. Default Date.now.
  now?: () => number;
}

// Owns arm state + throttling. Safety rule: whenever disarmed (including the
// disarm transition, emergency stop and explicit stop()) a v=0 command is sent
// immediately, bypassing the throttle, so the drone never coasts on a stale value.
export class DroneController {
  private armed = false;
  private lastSentAt = -Infinity;
  private lastCmd: ControlCommand | null = null;

  constructor(private opts: DroneControllerOpts) {}

  private clock(): number {
    return (this.opts.now ?? Date.now)();
  }

  isArmed(): boolean {
    return this.armed;
  }

  lastCommand(): ControlCommand | null {
    return this.lastCmd;
  }

  // Arm / disarm. Disarming forces an immediate v=0 (safety).
  setArmed(armed: boolean): ControlCommand {
    this.armed = armed;
    return this.send(armed ? focusToCommand(this.lastFocus) : 0, true);
  }

  private lastFocus = 0;

  // Feed the latest focus each frame. Returns the command actually sent, or
  // null if throttled. Sends 0 while disarmed.
  update(focus: number): ControlCommand | null {
    this.lastFocus = focus;
    const v = this.armed ? focusToCommand(focus) : 0;
    if (this.clock() - this.lastSentAt < (this.opts.minIntervalMs ?? 100)) return null;
    return this.send(v, false);
  }

  // Emergency stop: disarm and force v=0 immediately.
  stop(): ControlCommand {
    this.armed = false;
    return this.send(0, true);
  }

  private send(v: number, _force: boolean): ControlCommand {
    const cmd: ControlCommand = { v, arm: this.armed, ts: this.clock() };
    this.opts.publish(JSON.stringify(cmd));
    this.lastSentAt = cmd.ts;
    this.lastCmd = cmd;
    return cmd;
  }
}
