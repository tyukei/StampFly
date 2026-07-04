#!/usr/bin/env python3
"""Subscribe to the StampFly drone MQTT topics and print what flows through.

Use it to verify the web UI (simulator or real EEG) is actually publishing
control commands, without needing the drone powered on.

  pip install paho-mqtt
  python3 scripts/mqtt_monitor.py                    # default topic stampfly/demo
  python3 scripts/mqtt_monitor.py --topic stampfly/x # match a custom UI topic

Connects to the same public broker the browser uses (HiveMQ) over plain TCP.
"""
import argparse
import json
import time

import paho.mqtt.client as mqtt

BROKER = "broker.hivemq.com"
PORT = 1883


def on_connect(client, userdata, _flags, reason_code, *_):
    sub = f"{userdata}/#"
    print(f"[connected rc={reason_code}] subscribing {sub}")
    client.subscribe(sub)


def on_message(_client, _userdata, msg):
    ts = time.strftime("%H:%M:%S")
    try:
        payload = json.loads(msg.payload.decode())
    except Exception:
        payload = msg.payload.decode(errors="replace")
    print(f"{ts}  {msg.topic}  {payload}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--topic", default="stampfly/demo", help="topic base (match the web UI)")
    ap.add_argument("--broker", default=BROKER)
    ap.add_argument("--port", type=int, default=PORT)
    args = ap.parse_args()

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, userdata=args.topic)
    client.on_connect = on_connect
    client.on_message = on_message
    print(f"connecting to {args.broker}:{args.port} …")
    client.connect(args.broker, args.port, keepalive=30)
    client.loop_forever()


if __name__ == "__main__":
    main()
