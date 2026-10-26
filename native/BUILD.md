# Windows build: Native DLL and Launcher

Run from the repository root in a **Visual Studio 2022 Developer PowerShell** shell:

```powershell
cmake -S native -B native/build -A x64
cmake --build native/build --config Release
```

Output:
- `native/build/Release/BedrockNative.dll` — a real Win64 DLL with exported version/menu functions.
- `native/build/Release/BedrockLauncher.exe` — a native GUI that launches the installed Minecraft Bedrock through `minecraft://`.

These two binaries are independently buildable from the legacy overlay app.

**Not yet available:** injection into Minecraft, hooking its frame rendering or keyboard input, reading camera/entities/inventory, 3D waypoints, working AutoTotem, AutoCrystal or Aimbot. Exported Right Shift functions need a real host to call them and do not install a keyboard hook.

Injecting a DLL into the retail Bedrock process may be restricted by app packaging, process protections and anti-cheat; compatibility is version dependent. This commit does not modify or bypass these protections. Do not mistake the launcher for the completed client.
