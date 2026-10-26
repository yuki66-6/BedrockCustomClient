# Native Bedrock client — development branch

This folder begins the migration from the existing external overlay toward a native Minecraft Bedrock client.

## Implemented in this commit
- Version-independent C++20 interfaces for real player, camera and entity snapshots.
- Right Shift (VK_RSHIFT) edge-triggered ClickGUI toggle state, without key repeat.
- Validity flags to prevent treating unavailable telemetry as genuine data.

## Not implemented yet
- DLL injection, hooks, memory offsets or renderer integration.
- Connection to Minecraft's internal camera, inventory or entities.
- In-game 3D waypoints, minimap, AutoTotem, AutoCrystal or aim control.

No module should claim in-game functionality until the provider is implemented for a tested Bedrock build. Keep the existing overlay build intact in main.

Next: isolate a version-specific backend; build a DLL with a safe unload lifecycle; verify rendering and input on the user's Windows build; then port map and combat modules.

Right Shift controls the menu. The X key is reserved for the user's optional Target Lock module.
