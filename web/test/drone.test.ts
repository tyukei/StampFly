import { describe, expect, it } from "vitest";
import { CMD_MAX, CMD_MIN, type ControlCommand, DroneController, focusToCommand } from "../src/drone";

describe("focusToCommand", () => {
  it("clamps non-positive / non-finite focus to 0 (safe default)", () => {
    expect(focusToCommand(0)).toBe(CMD_MIN);
    expect(focusToCommand(-1)).toBe(CMD_MIN);
    expect(focusToCommand(NaN)).toBe(CMD_MIN);
    expect(focusToCommand(Infinity)).toBe(CMD_MIN);
  });

  it("maps focus 0..2 to 0..10 linearly", () => {
    expect(focusToCommand(1)).toBeCloseTo(5, 6);
    expect(focusToCommand(2)).toBeCloseTo(10, 6);
  });

  it("clamps large focus to CMD_MAX", () => {
    expect(focusToCommand(5)).toBe(CMD_MAX);
  });

  it("is monotonic non-decreasing in the active range", () => {
    let prev = -1;
    for (let f = 0; f <= 2; f += 0.1) {
      const v = focusToCommand(f);
      expect(v).toBeGreaterThanOrEqual(prev);
      prev = v;
    }
  });
});

// Deterministic clock helper for throttle tests.
function makeClock(start = 0) {
  const state = { t: start };
  return { now: () => state.t, advance: (ms: number) => (state.t += ms) };
}

describe("DroneController", () => {
  it("sends 0 while disarmed regardless of focus", () => {
    const sent: string[] = [];
    const clk = makeClock();
    const c = new DroneController({ publish: (p) => sent.push(p), now: clk.now });
    const cmd = c.update(2.0);
    expect(cmd).not.toBeNull();
    expect((cmd as ControlCommand).v).toBe(0);
    expect((cmd as ControlCommand).arm).toBe(false);
  });

  it("emits the mapped value once armed", () => {
    const clk = makeClock();
    const c = new DroneController({ publish: () => {}, now: clk.now });
    c.setArmed(true);
    clk.advance(100); // past the throttle window opened by setArmed's immediate send
    const cmd = c.update(2.0);
    expect(cmd?.v).toBeCloseTo(10, 6);
    expect(cmd?.arm).toBe(true);
  });

  it("throttles to minIntervalMs", () => {
    const sent: ControlCommand[] = [];
    const clk = makeClock();
    const c = new DroneController({
      publish: (p) => sent.push(JSON.parse(p)),
      now: clk.now,
      minIntervalMs: 100,
    });
    c.setArmed(true); // immediate send (force)
    sent.length = 0;
    expect(c.update(1.0)).toBeNull(); // 0ms since last → throttled
    clk.advance(50);
    expect(c.update(1.0)).toBeNull(); // 50ms → still throttled
    clk.advance(60);
    expect(c.update(1.0)).not.toBeNull(); // 110ms → sent
    expect(sent.length).toBe(1);
  });

  it("disarm forces an immediate v=0 bypassing the throttle", () => {
    const sent: ControlCommand[] = [];
    const clk = makeClock();
    const c = new DroneController({ publish: (p) => sent.push(JSON.parse(p)), now: clk.now });
    c.setArmed(true);
    c.update(2.0);
    sent.length = 0;
    const cmd = c.setArmed(false); // no clock advance, but must still fire
    expect(cmd.v).toBe(0);
    expect(cmd.arm).toBe(false);
    expect(sent).toHaveLength(1);
    expect(sent[0].v).toBe(0);
  });

  it("stop() disarms and forces v=0", () => {
    const sent: ControlCommand[] = [];
    const c = new DroneController({ publish: (p) => sent.push(JSON.parse(p)) });
    c.setArmed(true);
    sent.length = 0;
    const cmd = c.stop();
    expect(cmd.v).toBe(0);
    expect(c.isArmed()).toBe(false);
    expect(sent[sent.length - 1].v).toBe(0);
  });
});
