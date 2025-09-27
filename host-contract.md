## Host–Device JSON Contract (v1.0)

This document defines the JSON contract between the Nano_D firmware and an external host application. The device provides input, haptics, and display; the host performs OS-level audio actions (volume, output/input device changes, mute).

### Transport and Framing
- Default transport: USB CDC Serial. HID may be added later with identical JSON payloads.
- Framing: newline-delimited JSON (one message per line, UTF‑8).
- Device emits a hello on startup for discovery:

```json
{"hello":{"version":"1.0","transports":["cdc"],"product":"Nano_D++"}}
```

### Device → Host events
- Mode enter

```json
{"evt":"mode.enter","mode":"volume"}
{"evt":"mode.enter","mode":"output"}
{"evt":"mode.enter","mode":"input"}
{"evt":"mode.enter","mode":"wildcard"}
```

- Mute toggle (momentary action)

```json
{"evt":"mute.toggle"}
```

- Volume slot selection (Button A–D in volume mode; 0=A, 1=B, 2=C, 3=D)

```json
{"evt":"volume.select.slot","slot":0}
```

- Dial value (volume mode). Absolute, clamped to configured range/step.

```json
{"evt":"dial","value":37}
```

- Selection index (output/input/wildcard). Zero-based, clamped to [0..N-1].

```json
{"evt":"select.index","index":3}
```

- Confirm (Button A in selection modes; includes current index)

```json
{"evt":"confirm","index":3}
```

### Host → Device UI/state
- Set UI mode and optional title/subtitle and selection metadata

```json
{"ui":{"mode":"volume","title":"Volume","subtitle":"Spotify","index":0,"count":0}}
{"ui":{"mode":"output","title":"Select Output","subtitle":"MacBook Speakers","index":1,"count":5}}
```

- Dial range and current value (sets haptic detents and absolute mapping)

```json
{"dial":{"range":{"min":0,"max":100,"step":1},"value":37}}
```

- Selection list for output/input/wildcard (labels are shown on LCD; length sets detents)

```json
{"list":{"type":"output","items":[
  {"id":"BuiltIn","label":"MacBook Speakers"},
  {"id":"HDMI-1","label":"Monitor"},
  {"id":"BT-1234","label":"AirPods"}
],"index":0}}
```

- Temporary overlay (e.g., after mute/confirm)

```json
{"overlay":{"text":"Muted","timeout_ms":2000}}
```

- Device settings (optional; persisted in SPIFFS). To read, send {"settings":""} and the device replies with the current settings JSON.

```json
{"settings":{"idleTimeout":15000,"ledMaxBrightness":100}}
{"save":true}
{"load":true}
```

### Behavior and State
- Modes: volume, output, input, wildcard.
- Selection modes: turning the knob emits select.index; pressing Button A emits confirm and returns to volume (for output/input).
- Wildcard: confirm binds the selected target, then device remains in volume for that target.
- Mute-toggle: emits mute.toggle; device shows a short overlay and remains in prior mode.
- Clamping:
  - Dial: value mapped using min/max/step; out-of-range clamped to [min..max].
  - Selection: indices clamped to [0..items.length-1].
- LCD:
  - Volume: shows title/subtitle and arc value.
  - Selection: shows title/subtitle and index/count (e.g., 2/5); short overlay on confirm.

### Recommended host flow
1) On hello, connect and send initial UI for the default mode.
2) On {"evt":"mode.enter","mode":"volume"}:
   - Send target label in ui.subtitle and dial range/value.
3) On {"evt":"volume.select.slot","slot":k}:
   - Bind to app slot k; send updated ui.subtitle and dial.
4) On {"evt":"mode.enter","mode":"output"|"input"|"wildcard"}:
   - Send list with items and default index.
5) On {"evt":"select.index","index":i}:
   - Update preview; do not apply until confirm.
6) On {"evt":"confirm","index":i}:
   - Apply selection (switch OS device or bind target), then push updated volume ui and dial.
7) On {"evt":"mute.toggle"}:
   - Toggle OS mute; optionally send overlay.

### Discovery
- Match by VID/PID and product string; use CDC Serial. A HID transport may be added later with identical message bodies.

### Chord mappings (defaults; configurable later via settings)
- A+B → volume
- B+C → output
- C+D → input
- A+D → wildcard
- A+C → mute

### Errors
- On parse failure, device emits:

```json
{"error":"JSON parse error","msg":"..."}
```

Host should ignore unknown fields and tolerate additional keys for forward compatibility.





