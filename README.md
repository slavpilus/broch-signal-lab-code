# broch-signal-lab-code

Public companion code for **Broch Signal** — a learning-in-public series on LoRaWAN, Meshtastic, and rural-Scotland LPWAN. Firmware, gateway configs, data-pipeline bits, Grafana dashboards, and SVG diagrams that accompany each lesson.

**→ Read the field notes at [www.brochsignal.com](https://www.brochsignal.com/)**

Every lesson in the LoRaWAN / Meshtastic learning series has a matching **tagged release** of this repo. Posts on the blog link to a specific tag so you can reproduce the work at the exact state I had on the bench when I wrote it.

## Layout

Organised by phase, then lesson:

```
phase-0/                       # foundations (no code yet)
phase-1/
  lesson-1.1-hello-world/      # PlatformIO projects, serial logs, settings
  lesson-1.2-range-walk/
  lesson-1.3-three-knobs/
phase-2/
  ...
diagrams/                      # shared SVGs referenced from multiple posts
```

## Tag convention

| Tag    | Lesson                                  |
|--------|-----------------------------------------|
| `v1.1` | Phase 1 · Lesson 1.1 — Hello world      |
| `v1.2` | Phase 1 · Lesson 1.2 — First range walk |
| ...    |                                         |

Each tag is annotated with the lesson title and a link to the published post.

## Region

EU868 throughout. If you're outside Europe, you'll need to rebuild for your region (US915, AS923, etc.) — the radio settings differ.

## Licence

MIT (see `LICENSE`).
