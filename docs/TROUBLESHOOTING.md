# Troubleshooting

User-facing. Ships in the zip once there is one. Until the first build exists the entries below
are the ones the sibling mods needed on day one, kept so the first tester build does not
rediscover them.

## The headset shows nothing and the log says `xrCreateInstance failed -32`

A 64-bit OpenXR API layer is installed system-wide (OBS Studio's is the usual one) and it
cannot load into a 32-bit game. The mod detects the known ones and disables them for its own
process; if the line still appears, the log names the layer. Disabling that layer's
"implicit" registration for the session, or launching without OBS, clears it.

## SteamVR headsets

SteamVR has no 32-bit OpenXR runtime. The mod ships a small bridge DLL that talks to SteamVR
through OpenVR. The log's runtime line should name it. If SteamVR crashes at startup, try
with the desktop mirror on.

## Quest over Virtual Desktop

Use Virtual Desktop's own OpenXR runtime (VDXR). The log's `xr: instance created on runtime`
line says which runtime answered; if it is not VDXR, the ini's runtime setting is overriding
the system choice.

## The game does not start at all

Launch through Steam, not the exe directly: the game expects the Steam client. If it exits
silently in the first seconds with the mod installed and the log stops after the hooks are
installed, that is a CEG interaction and it belongs in a bug report with the log attached.
Delete the mod's DLL from the game folder to confirm the game runs without it.

## Clearing the mod's settings

Rename `darkness2_vr.ini` next to the exe and launch once. The mod writes a fresh one with the
shipped defaults. Do this before reporting a bug so the report is against known settings.
