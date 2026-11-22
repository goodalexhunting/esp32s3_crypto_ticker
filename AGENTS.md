# Repository guide

This repository contains the ESP32-S3 Crypto Ticker firmware, its embedded Config UI, a GitHub Pages landing page, and host and hardware-in-the-loop checks. Keep changes consistent with the device behavior documented in [README.md](README.md), the vocabulary in [GLOSSARY.md](GLOSSARY.md), and applicable decisions in [docs/adr](docs/adr/).

## Work sequence

1. Read this guide and the relevant glossary terms and ADRs before changing a subsystem.
2. Trace the behavior from its entry point through the owning module and its callers. Prefer the existing module boundary and test seams.
3. Make the smallest coherent change. Keep firmware behavior, embedded web content, release assets, and user documentation aligned when they describe the same contract.
4. Run the relevant build or checks when asked to verify, or when verification is part of the requested task. Report commands and outcomes accurately; distinguish unavailable hardware checks from host checks.

## Architecture and invariants

- `src/main.cpp` owns boot ordering and the cooperative device loop. Hardware-facing modules live in `src/` with public interfaces in `include/`.
- `ConfigManager` owns ticker configuration and NVS persistence. Async Config UI handlers enqueue operations; the main loop applies mutations so shared configuration stays single-threaded.
- `include/app_config.h` owns firmware-wide constants. `include/embedded_ui.h` owns the embedded Config UI and AP setup page. `site/` owns the public landing page.
- OTA safety depends on the factory plus two OTA slots, SHA-256 verification, the boot watchdog, and self-test verification. Read [ADR 0001](docs/adr/0001-ota-ab-rollback.md) before changing OTA or partitions.
- Keep pure and host-testable logic behind existing helpers and seams. Native test suites include implementation `.cpp` files directly and use `test/mocks/` for hardware APIs.
- Preserve the glossary's domain terms in code, tests, and docs. A batch price fetch uses one quote currency; each ticker's label does not identify its CoinGecko API id.

## Commands

- Firmware build: `pio run -e lilygo-t-display-s3`
- Native tests: `pio test -e native`
- One native suite: `pio test -e native -f <suite_name>`
- Hardware-in-the-loop instructions: [test_hw/README.md](test_hw/README.md)

For C++ changes, follow [CODING_STANDARDS.md](CODING_STANDARDS.md). The issue tracker conventions are in [docs/agents/issue-tracker.md](docs/agents/issue-tracker.md), including the canonical labels in [docs/agents/triage-labels.md](docs/agents/triage-labels.md). Domain documentation conventions are in [docs/agents/domain.md](docs/agents/domain.md).
