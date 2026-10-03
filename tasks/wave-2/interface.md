# Wave 2: interface

## Goal

The page: the effect browser, the inspector, the timeline, the viewer and the
transport, all driven by the catalogue the host sends rather than by a list the
page keeps. Substantially done — the browser groups by category, the timeline
draws tracks and clips, the transport scrubs, and the layout reports itself
clean. What remains is the interaction that needs a real project to exercise:
dragging clips, editing keyframes on the timeline, and the expression field's
live validation.

## Territory

- src/ui/**

## Deliverable

- The layout report shows no collapsed band and nothing past the bottom edge,
  at every window size from 1024x700 up.
- Every effect in the catalogue is reachable from the browser and its
  parameters are editable.
- A parameter with keyframes shows a stopwatch that reflects its state and a
  keyframe lane on the timeline.
- The page never edits its own copy of the document: every change is a message
  to the host and a fresh snapshot back.

## Notes

The page carries no list of effects, no parameter ranges and no labels. All of
that comes from the catalogue message, so adding an effect in `core` makes it
appear here with no change to this territory. That is the property worth
protecting: a page that knows about specific effects is a page that drifts out
of step with the renderer.

The layout report (`window.__keyflowLayoutReport`) is the interface's own test.
It measures every band and names the ones that collapsed or fell off the
bottom, which turns a layout bug into a list of numbers.
