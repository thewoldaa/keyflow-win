# Tasks

One file per task, in the wave it belongs to. A task file declares the files
that task owns; the harness refuses to start a task whose territory overlaps
another task in the same wave.

    scripts/harness/wave-new.ps1 -Wave 1 -Task renderer

## Format

    # Wave N: <name>

    ## Goal

    One paragraph. What is true when this is done.

    ## Territory

    - src/gl/**
    - tests/RenderTests.cpp

    ## Deliverable

    - Bullet points a reviewer can check.

## Territory syntax

- `path/to/file.ext` — exactly that file
- `path/to/dir/*` — the direct children
- `path/to/dir/**` — that directory and everything under it

Nothing else. Wildcards inside a path segment are rejected on purpose:
supporting them would mean implementing glob intersection, and the check is
meant to be obviously correct rather than expressive.

## Why territories

Two tasks writing the same file is the failure the harness exists to prevent.
A conflict found before a worktree is created costs nothing; the same conflict
found after a day of parallel work costs the day.
