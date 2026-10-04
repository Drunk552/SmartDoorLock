# Project Baseline

## Current Repository State

As inspected on 2026-10-04:

- The repository has two owned Markdown baselines and a large `资料/` vendor-material tree.
- Git is not initialized.
- No owned CubeMX `.ioc`, Keil project, build system, or firmware source exists outside vendor examples.
- The correct next project gate is hardware-baseline closure followed by the minimum runnable firmware, not full multi-module integration.

## Source Hierarchy

Use sources in this order when they disagree:

1. The user's current request and measured evidence from the actual board.
2. Frozen project decisions in `智能门锁硬件接线与接口定义文档.md` and `STM32F103智能门锁离线控制系统开发文档.md`.
3. The exact module's matching datasheet, schematic, and seller documentation under `资料/`.
4. Closely related vendor examples under `资料/`.
5. Generic online examples or assumptions.

Record discrepancies instead of silently choosing a convenient value.

## Confirmed Scope

Target controller: STM32F103C8T6, designed to the official 64 KB Flash, 20 KB SRAM, 72 MHz baseline.

Required offline functions:

- password, RFID, and fingerprint authentication;
- OLED status/menu interaction;
- AT24C02 configuration persistence;
- DS3231 offline time;
- timed automatic relock;
- failed-attempt lockout;
- administrator management of password, RFID cards, and fingerprint IDs;
- buzzer/status indication, fault handling, and watchdog protection;
- a reserved UART protocol for a future intelligent module.

Optional later work: door sensor, exit button, W25Qxx event log, network or face-module integration.

Out of the current offline release: mobile app, cloud platform, remote unlock, 4G, implemented face recognition, video, and cloud user database.

## Hardware Baseline

- OLED: 0.96 inch, 128 x 64, four-wire I2C, project address `0x3C` (HAL address `0x78`), PB6/PB7. The supplied material labels the controller as SSD1315 in one specification section but also contains an SSD1306 block diagram. Verify initialization and command compatibility on the actual display.
- AT24C02: 256 bytes, project assumes an 8-byte write page, normally address `0x50`, shared PB6/PB7 I2C bus.
- DS3231: address `0x68`, battery backed, module confirmed without a charging circuit; battery type still needs recording.
- MFRC522/RC522: 3.3 V SPI1 on PA4-PA7 with reset on PB0.
- AS608: project supply is 3.3 V on USART2 PA2/PA3. The supplied ATK manual describes 3.0-3.6 V, default 57600 8N1 TTL, typical 40 mA, and 300 templates, but connector pinout, logic-high voltage, peak current, and the exact board variant must be checked on the physical module.
- LYO3C: 12 V, 0.4 A, never GPIO-driven; use a 3.3 V-compatible logic MOSFET or verified relay circuit with flyback protection. Whether power means lock or unlock is unresolved.
- SWD: keep PA13 and PA14 reserved.
- First prototype: keypad PB8-PB15; debug USART1 PA9/PA10; lock control candidate PA8; buzzer candidate PB1.

## Unresolved Gate-0 Facts

Do not freeze the schematic or connect the real lock until these are resolved:

- development-board schematic, 5 V input path, 3.3 V regulator type/current, onboard LED polarity/pin;
- AS608 connector order, UART voltage, default baud confirmed on this unit, idle/peak current;
- LYO3C energized behavior, measured startup/steady current, internal protection;
- exact MOSFET/relay driver and power supply;
- AT24C02 address pins and write-protect state;
- DS3231 battery type and whether another EEPROM is fitted;
- exact keypad ribbon order and buzzer type.

## Development Stages

0. Hardware facts, voltage/current budget, pin table, staged power-up checklist.
1. CubeMX project, SWD, LED, USART1 logs, OLED boot display; one-hour stability.
2. Keypad password flow with LED-simulated non-blocking five-second unlock.
3. DS3231 plus AT24C02 versioned configuration, CRC, page-safe writes, and power-cycle recovery.
4. RC522 UID reading and authorized-card management.
5. AS608 search, enrollment, deletion, ID mapping, and bounded timeouts.
6. Administrator menu, failure lockout, alarm, and watchdog.
7. Staged real-lock driver integration and 100-cycle stability test.
8. Optional door sensing and external-flash circular event log.
9. Optional reserved UART protocol with malformed-frame tests.

Do not start several new hardware modules at once. Each stage must leave a working, testable system.

## Owned Baseline Documents

- `STM32F103智能门锁离线控制系统开发文档.md`: scope, architecture, stages, coding rules, testing, Git, release, and thesis material.
- `智能门锁硬件接线与接口定义文档.md`: power, pin allocation, wiring, staged bring-up, BOM, and unresolved physical facts.

Use the large documents contextually: read hardware sections for wiring changes, storage sections for EEPROM work, test sections for verification, and release sections for packaging.
