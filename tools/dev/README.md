# Lean Project Commands

Run from the active checkout in PowerShell. No helper starts a game implicitly.
`context.ps1` is read-only and works from any current directory:

```powershell
./tools/dev/context.ps1
./tools/dev/context.ps1 -Mode headings
./tools/dev/context.ps1 -Mode notes -Section 'Radial Pedals' -MaxLines 35
./tools/dev/test-helpers.ps1
```

Native command output stays in an ignored, uniquely named log. Success emits one
line; failure emits that line plus a bounded tail and preserves the exit code.
Always check completion/exit status before dependent commands. Read the log when
the summary is insufficient. Supply executable arguments as an array, not shell text.
Do not pass credentials in arguments/output. This helper does not mute games.

```powershell
./tools/dev/run-logged.ps1 -Label windows -Command cmake -CommandArgs @('--build','build/x64','--config','Debug','--target','Spaghettify','--parallel','8')
./tools/dev/run-logged.ps1 -Label android -WorkingDirectory ./android -Command ./gradlew.bat -CommandArgs @('assembleRelease','--no-daemon','--max-workers=2')
```

Focused native checks (no game or phone launch): choose only the affected suite.
Names: `audio`, `hud`, `menu_pointer`, `network`, `vr_refresh`. Network tests use
local ENet loopback; initial configuration can download the pinned dependency.

```powershell
cmake -S tests/hud -B build/hud-tests -G 'Visual Studio 17 2022' -A x64
cmake --build build/hud-tests --config Debug
./tools/dev/run-logged.ps1 -Label hud -Command ctest -CommandArgs @('--test-dir','build/hud-tests','-C','Debug','--output-on-failure')
```

Desktop executable: `build/x64/Debug/Spaghettify.exe`.
Phone APK: `android/app/build/outputs/apk/release/app-release.apk`.
Phone SDK/toolchain and explicit deploy instructions: `android/README.md`.
Discover a current serial with `adb devices -l`; do not reuse old Wi-Fi ports.

Historical notes are retained, not automatically loaded. Keep `AGENTS.md` concise
and route deeper work to its topic map. Instruction scoping reference:
[OpenAI Docs](https://learn.chatgpt.com/docs/agent-configuration/agents-md).
