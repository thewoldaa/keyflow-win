# Wave 3: pipeline

## Goal

Decode, cache and encode: the frame cache that makes scrubbing usable, the
render queue that keeps the window responsive, audio, and the export dialog's
settings actually reaching ffmpeg. Substantially done — import probes a file
and adds a layer, and a render walks frames, writes PNGs and encodes. What
remains is the cache (a decode per layer per frame is the current cost) and
audio.

## Territory

- src/media/**
- src/app/**

## Deliverable

- Scrubbing a 4K clip holds a usable frame rate, because a decoded frame is
  cached by (asset, time) and reused.
- A render runs on a worker thread, reports progress, and can be cancelled
  without leaving a partial file behind.
- Import reports what ffprobe found, and a file that will not probe produces a
  clear message rather than a silently empty layer.
- The export dialog's codec, quality and size reach the encoder.

## Notes

ffmpeg is a runtime dependency rather than a linked one: the build has no
third-party link step, a decoder crash cannot take the editor with it, and the
user almost certainly already has it. The cost is a process spawn per decode,
which is what the frame cache exists to pay down.

The render writes a numbered PNG sequence and then encodes it, rather than
piping raw frames to ffmpeg's stdin. Piping needs the render and the encode to
stay in lockstep, and a stall on either side deadlocks both.
