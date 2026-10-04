# Development Workflow

## Closed Loop

For each milestone, use this loop:

1. Baseline: inspect current owned files, Git state, and relevant hardware facts.
2. Requirement: assign or confirm a requirement ID, behavior, error paths, and measurable acceptance criteria.
3. Design: define module boundary, API, state transitions, timeouts, memory/storage effects, and pin/power impact.
4. Implement: build the smallest vertical slice and keep generated code boundaries intact.
5. Verify locally: compile where the real toolchain is available; run host tests, static checks, and focused reviews that are possible without hardware.
6. Verify physically: give the user exact wiring, measurement, flashing, serial-log, and repetition steps. Wait for returned evidence when a physical result is decisive.
7. Correct: analyze failures, make a focused change, and rerun affected checks.
8. Record: update requirement traceability, interface/storage documents, test evidence, known limitations, and relevant diagrams.
9. Review: audit authorization, bounds, timeout paths, ISR behavior, power-up safety, and resource use.
10. Integrate/release: commit a coherent unit, run regression, and package reproducible artifacts only after the milestone gate passes.

## Evidence Standard

Use evidence labels:

- `AUTO-PASS`: a command or automated test actually passed in the current environment.
- `BENCH-PASS`: the user supplied a hardware log, measurement, photo, or repeat count that meets the criterion.
- `REVIEW-PASS`: a documented inspection passed but does not substitute for execution.
- `PENDING-HW`: requires board access or user action.
- `BLOCKED`: a necessary fact or tool is missing and no defensible workaround remains.

Do not collapse `PENDING-HW` into pass.

## Firmware Design Checks

- Keep hardware drivers below services and application logic.
- Return explicit result codes; preserve failure reasons.
- Use wrap-safe elapsed-time comparisons for tick counters.
- Use explicit timeouts for UART, I2C, SPI, enrollment, polling, and lock actuation.
- Keep interrupt handlers short and move parsing/UI/storage work to the main context.
- Validate lengths, IDs, indexes, addresses, CRCs, and configuration versions before use.
- Keep a single owner for lock output and a single owner for EEPROM addresses.
- Initialize lock GPIO to the configured safe level before other peripherals.
- Avoid dynamic allocation unless the project explicitly justifies it.
- Budget Flash, SRAM, stack, EEPROM wear, and log capacity against STM32F103C8T6 and AT24C02 limits.

## Hardware Bring-Up Checks

- Verify rail voltages before attaching logic modules.
- Add one module at a time and keep a known-good baseline.
- Run I2C scanning only after checking combined pull-up resistance.
- Use LED simulation before attaching the lock driver, then test the driver without the lock, then attach the lock last.
- Separate lock-current wiring from I2C/SPI/UART wiring and verify common-ground strategy.
- Record voltage and current at idle, recognition, and lock actuation.
- Treat unexplained resets, OLED flicker, RFID dropouts, or UART corruption during lock motion as failed power/EMI integration.

## Definition of Done for a Milestone

A milestone is complete only when its requirement and acceptance criteria exist, implementation and error paths are reviewed, available builds/tests pass, physical requirements have `BENCH-PASS` evidence, resource impact is acceptable, and relevant documents/traceability are updated. Report every unmet item explicitly.
