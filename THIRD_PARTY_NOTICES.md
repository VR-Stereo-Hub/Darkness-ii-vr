# Third-party notices

Nothing is vendored yet; this file records what the planned framework will bring in, and the
posture it is brought in under, so the first framework PR does not have to decide it.

| Component | Licence | Use |
|---|---|---|
| Dear ImGui | MIT | The F10 panel (`third_party/imgui`, submodule) |
| OpenXR SDK (Khronos) | Apache 2.0 | The static OpenXR loader linked into the mod module (`third_party/OpenXR-SDK`, submodule) |
| OpenVR (Valve) | BSD-3-Clause | Headers and `openvr_api.dll` for the 32-bit SteamVR shim (vendored, hash-checked) |
| LibLZF (Marc Lehmann) | BSD-2-Clause | Decompressing the game's `.cache` blocks in the read-only extractor (planned) |
| MinHook | BSD-2-Clause | Only if a detour library is adopted; Dishonored uses byte-verified 5-byte detours without one |

## Provenance from the sibling mods

The OpenXR runtime layer (`core/vr/openxr_runtime`, `openxr_input`), the simulated 32-bit
OpenXR runtime (`xrsim`) and the SteamVR shim are adopted from the BioShock trilogy VR mod by
way of the Dishonored VR mod, both in the same org, and stay as close to those copies as the
host allows so fixes port between the three projects.

## What is not used

- No code from UEVR (all rights reserved; concepts only).
- REFramework (MIT) may be adapted with an attribution comment.
- No game content of any kind: no extracted assets, no decompiled Lua, no shader dumps, no
  captures. The game's own strings quoted in `docs/` are identifiers, not content.
