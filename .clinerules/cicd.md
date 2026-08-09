### CI/CD and Versioning Rules

Use a three-stage promotion model: 

dev → staging → prod 

### Branches

* dev is for active development.
* staging is for testing release candidates.
* prod is the production branch.
* Production firmware/releases must only be built and published when changes reach prod.
* Do not create production GitHub Releases from dev or staging.

### Versioning

Use semantic versioning: 

MAJOR.MINOR.PATCH 

Examples: 

* 1.0.0 — initial/version 1 release
* 1.1.0 — minor feature update
* 1.1.1 — patch/bugfix
* 2.0.0 — major/breaking release

### Version Control & Automation

* **User Managed**: Major and minor versions must be explicitly controlled and manually managed by the developer. CI must not automatically increment major/minor versions based on commits or pushes.
* **Automated Patches**: Any regular push or merge into the prod branch must automatically increment the PATCH number for the release (e.g., 1.1.0 → 1.1.1).

### Releases

When changes are merged into prod: 

1. Read the user-configured Major and Minor version, and apply the autoincremented Patch number.
2. Build the PlatformIO firmware.
3. Build the LittleFS filesystem image.
4. Create a GitHub production Release using that combined version.
5. Attach the firmware and filesystem binaries as release assets.
6. Make the release available for ESP32 OTA updates.

Development/nightly builds may still be produced for testing, but they must be clearly separated from production releases and must not be treated as stable OTA releases. 

### ESP32 OTA

The ESP32 should only consider stable production releases when checking for OTA updates. 

The firmware version should be available to the application so the device can report its currently installed version. 

Keep the firmware version, GitHub Release version, and OTA version consistent. 

### Important

Do not redesign or introduce automatic semantic-versioning conventions without first inspecting the existing CI/CD configuration and repository structure.