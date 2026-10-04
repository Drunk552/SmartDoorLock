---
name: smart-door-lock-graduation
description: Plan, implement, verify, document, and present this STM32F103 smart-door-lock graduation project. Use for project milestones, firmware work, hardware bring-up, testing, releases, thesis evidence, or defense preparation in this repository.
---

# Smart Door Lock Graduation Project

Move the project to the next verifiable milestone while keeping firmware, hardware facts, test evidence, and thesis claims consistent.

## Start Here

1. Identify whether the request concerns planning, hardware confirmation, firmware implementation, debugging, verification/release, thesis writing, or defense preparation.
2. Read [references/project-baseline.md](references/project-baseline.md) for the current scope, source hierarchy, stage map, and unresolved hardware facts.
3. Read only the relevant project document sections and files. Do not scan every vendor example for ordinary work.
4. For a request about how to prompt Codex or how to run the whole project, read [references/prompt-playbook.md](references/prompt-playbook.md).
5. For implementation, verification, or release work, follow [references/development-workflow.md](references/development-workflow.md).

## Project Rules

- Treat `资料/` as read-only vendor material. Copy or adapt only the minimum needed code into the owned firmware tree, and record its origin and any changes.
- Prefer observed hardware measurements and successful bench tests over generic module assumptions. Do not turn unverified values into confirmed facts.
- Never claim a hardware test passed unless the user supplied evidence or the test was actually run against accessible hardware.
- Keep the offline system usable without ESP32, cloud, mobile app, or face recognition. Those are extensions unless the user changes the approved scope.
- Preserve SWD pins PA13/PA14 and put the lock-control output into its configured safe state at the earliest initialization point.
- Do not drive the 12 V lock from an MCU GPIO. Require staged power and lock-driver checks before real-lock testing.
- Use non-blocking application logic. Hardware transactions may wait only with explicit, bounded timeouts.
- Centralize EEPROM layout and lock actuation decisions; drivers must not decide authorization.
- Keep code compatible with the actual generated STM32 project and toolchain. Do not invent CubeMX-generated files or build results.
- Use requirement IDs, acceptance criteria, and evidence for milestone work. Keep thesis prose honest about implemented, interface-only, and proposed features.

## Working Modes

### Assess or Plan

Report the current state from repository evidence, the nearest safe milestone, prerequisites, risks, acceptance criteria, and the smallest useful next action. Separate confirmed facts, assumptions, and user measurements still needed.

### Implement a Milestone

Restate the milestone and acceptance criteria, inspect the relevant owned code and documents, implement the smallest coherent vertical slice, and verify everything possible locally. If physical verification is required, finish with an exact bench-test checklist and the evidence the user should return.

### Diagnose

Reproduce from logs, code, wiring facts, and measurements. Rank hypotheses by evidence; run non-destructive checks first. Make a fix only when the user asks to fix or the request clearly includes implementation.

### Verify or Release

Trace requirements to code and tests, run available builds/checks, distinguish automated from manual hardware tests, update evidence artifacts, and audit the milestone against the Definition of Done. A release is not complete without recorded toolchain, artifacts, versions, known limitations, and test results.

### Thesis or Defense

Build claims from repository history, design documents, code, and test evidence. Mark each claim as implemented and verified, implemented but not fully verified, interface reserved, or future work. Never fabricate measurements, screenshots, citations, or experimental results.

## Completion Report

End milestone work with:

- outcome and files changed;
- checks run and their results;
- hardware checks still requiring the user;
- requirements/evidence updated or still missing;
- the next recommended milestone.
