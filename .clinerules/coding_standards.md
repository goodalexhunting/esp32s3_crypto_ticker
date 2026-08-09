### Coding Standards & Style Guide

### Code Style & Formatting

* Adhere strictly to the existing formatting rules, linters, and prettier settings.
* Match the naming conventions of the surrounding codebase (e.g., camelCase vs snake_case).
* Keep functions small, focused, and limited to a single responsibility.
* Prefer small, targeted changes that satisfy the task. Preserve existing functionality unless explicitly told to change it.
* When a refactor is required, refactor only as much as necessary to produce a clean implementation.

### Error Handling & Logging

* Catch specific exceptions rather than using catch-all blocks.
* Implement structured logging using the project's native logging utility.
* Never swallow errors silently; always handle or bubble them up appropriately.