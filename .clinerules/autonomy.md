### Autonomous Execution & Tool Rules

### Autonomous Execution

* Complete development tasks autonomously without requiring user interaction between normal implementation steps. The user may be AFK.
* Proceed autonomously: do not ask for permission before editing files or approving routine implementation decisions.
* Do not stop because a minor implementation detail was not explicitly specified.
* Make reasonable engineering decisions based on the current task, architecture, coding conventions, project rules, and resource constraints.
* Prefer a working, maintainable implementation over repeatedly discussing alternatives.
* Do not pursue hypothetical improvements after the requested functionality is working. Do not perform unrelated cleanup.

### Handling Requirement Ambiguity

* Do not stop for minor ambiguity. Make reasonable engineering decisions using project behaviour, dependencies, and tech-stack standards.
* **Ask the user for clarification ONLY when:** 

  * Two requirements directly contradict each other.
  * A destructive migration has multiple materially different outcomes.
  * Required credentials or secrets are unavailable.
  * A required hardware capability cannot be determined.
  * The requested behaviour is impossible without choosing between fundamentally different designs.
  * Continuing could destroy user data or repository history.

### Git & Working Tree Discipline

* Inspect the Git working tree before starting work.
* Always append --no-pager to git commands (e.g., git --no-pager log, git --no-pager diff) to ensure non-interactive, readable terminal output.
* If unrelated changes already exist, preserve them, do not revert them, do not reset them, and do not overwrite them.
* Do not include existing uncommitted user changes in the current task's commit.
* Work around existing changes where reasonably possible. If they conflict and cannot safely be preserved, stop and report the conflict.
* Never assume uncommitted changes were created by the agent. Do not discard or overwrite user work to simplify implementation.

### Tool and Terminal Usage

* Execute as many terminal commands as reasonably necessary to complete the task; there is no arbitrary limit.
* Prefer informative commands over redundant commands, and targeted searches over indiscriminate searches.
* Use non-interactive commands where possible, ensuring appropriate timeouts are applied.
* Prefer existing project tooling over ad-hoc tooling.
* Avoid repeatedly inspecting unchanged files or rerunning commands without a clear reason.
* Follow the standard development loop: Inspect → Edit → Build → Diagnose → Edit → Build/Test → Review → Commit.

### Repository & Project Boundaries

* Remain strictly within the current project repository.
* Reading unrelated files outside the project or searching the entire filesystem is prohibited.
* Accessing unrelated personal files or modifying unrelated projects is prohibited.
* Do not modify system configuration unless explicitly required by the task.