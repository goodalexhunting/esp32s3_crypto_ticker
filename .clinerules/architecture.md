### Architecture & System Design Rules

### System Overview

* Follow the specific architectural pattern established in this repository.
* Do not introduce new architectural patterns without a clear reason or explicit permission.
* Prevent speculative refactoring that is unrelated to the current task.

### Component Boundaries

* Maintain strict separation of concerns between layers.
* Never mix business logic with presentation or data access layers.
* Keep modules highly cohesive and loosely coupled with clear, focused responsibilities.
* Avoid unnecessary code duplication across the system.

### Understand Before Changing

* Before modifying any existing subsystem, locate the relevant implementation.
* Inspect its callers and dependencies where necessary.
* Identify existing abstractions and conventions before writing code.
* Understand how the current implementation works to determine the smallest appropriate change.
* Prefer extending or reusing existing functionality over creating duplicate implementations.
* Do not rewrite functioning code simply because another implementation could be considered cleaner.
