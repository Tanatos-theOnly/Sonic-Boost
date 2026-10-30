# Contributing to SonicBoost

Thank you for your interest in contributing to **SonicBoost**! We welcome community contributions, bug reports, performance enhancements, and feature requests.

---

## Code of Conduct

Please treat everyone with respect and kindness. Open, constructive feedback and collaborative problem solving are expected at all times.

---

## How to Contribute

### 1. Reporting Issues
* Check the GitHub Issue Tracker to ensure the problem has not already been reported.
* Provide a clear and descriptive issue title.
* Include your Windows version (e.g., Windows 10 22H2, Windows 11 23H2/24H2), sound card/DAC hardware, and reproduction steps.
* If reporting an audio glitch, specify whether it occurs under the In-Process Interception Engine, the APO Engine, or both.

### 2. Submitting Pull Requests
1. Fork the repository.
2. Create a feature branch for your changes:
   ```bash
   git checkout -b feature/amazing-feature
   ```
3. Ensure the project builds cleanly using `build.bat` without compiler warnings or linker errors.
4. Clean intermediate build files before committing:
   ```cmd
   clean.bat
   ```
5. Follow existing Win32 and modern C++17 coding conventions:
   * Keep functions focused and well-commented.
   * Maintain RAII and memory safety.
   * Respect thread synchronization for IPC and audio buffers.
6. Commit your changes with clear, concise commit messages.
7. Push your branch to GitHub and open a Pull Request.

---

## Building and Testing

SonicBoost requires the **Microsoft Visual C++ Build Tools** (Visual Studio 2022 recommended) with the Windows 10/11 SDK.

* Run `build.bat` from the root directory to compile both the native controller (`SonicBoost.exe`) and the 64-bit Audio Processing Object (`SoundBoosterApo.dll`).
* Run `clean.bat` to remove intermediate build artifacts (`*.obj`, `*.res`, `*.exp`, `*.lib`).

---

## Author & Maintainer

* **Reddy A. Rohan on earth**
