# Coding standards

These standards apply to firmware, tests, embedded web content, CI, and repository documentation. Follow established local patterns unless this file says otherwise; update the relevant test and documentation with any changed contract.

## C++ and firmware

- Format C++ with the repository `.clang-format` configuration: Google base, four-space indentation, no tabs, and a 100-column limit.
- Keep each module's public contract in `include/` and implementation in `src/`. Use the existing `cryptoapp` namespace for application modules.
- Keep `include/app_config.h` as the source of firmware-wide constants. Prefer named constants over unexplained literals for device behavior and timing.
- Keep the main loop cooperative. Bound blocking network work and preserve watchdog servicing and OTA self-test timing.
- Keep shared state single-threaded. Async handlers enqueue work for the main loop instead of mutating `ConfigManager` or display state directly.
- Use fixed-size storage for bounded device data where the owning module already follows that model. Check bounds at API edges and handle empty configuration safely.
- Preserve ticker identity as the combination of label, CoinGecko API id, quote currency, and optional color. Never derive an API id from a display label.
- Keep pure transformations separate from hardware I/O when practical, and extend the existing host seams instead of adding parallel test-only designs.

## Tests

- Add or update a native test for changed behavior when the behavior can be exercised without hardware. Name suites `test_<module>` under `test/` and use Unity conventions already present.
- Assert observable behavior and important boundaries: empty inputs, capacity limits, wraparound, malformed data, and failure recovery where relevant.
- Use the existing mocks and fake transport. Avoid coupling assertions to private implementation details unless the structure itself is an explicit safety invariant.
- Keep device-dependent checks under `test_hw/`; document required firmware flags, services, and device setup in `test_hw/README.md`.

## Web UI, CI, and documentation

- Treat `include/embedded_ui.h` and `site/` as separate delivery surfaces: the device serves the former, and GitHub Pages publishes the latter.
- Keep API paths, request/response shapes, and validation consistent between the embedded client, firmware handlers, and tests.
- Preserve branch and release gates in `.github/workflows/build.yml`. Production version extraction, artifact names, and OTA manifest fields are externally consumed contracts.
- Use `GLOSSARY.md` terminology. Keep the README user-facing, `AGENTS.md` operational, this file normative, and ADRs limited to durable decisions and tradeoffs.
- Update docs when behavior changes; avoid copying full architecture or command inventories across multiple documents.
