# SmartDoorLock Repository Guidance

- Use `.codex/skills/smart-door-lock-graduation/SKILL.md` for milestone implementation, hardware bring-up, verification/release, thesis evidence, or defense preparation.
- Treat `资料/` as read-only vendor material. Owned firmware and project documents must live outside it.
- The two root Chinese Markdown documents are the current project and hardware baselines. Read only the sections relevant to the task.
- Do not claim physical hardware success without user-provided or directly observed bench evidence.
- Preserve offline operation, PA13/PA14 SWD access, early safe lock-output initialization, bounded hardware timeouts, and non-blocking application behavior.
- Never drive the 12 V LYO3C lock directly from an STM32 GPIO. Require staged LED, driver-only, and real-lock tests.
- Keep generated STM32 code boundaries intact and avoid editing vendor examples in place.
- Keep requirements, interfaces, storage layout, tests, and thesis claims synchronized with implemented behavior.
