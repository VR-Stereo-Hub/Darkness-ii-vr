// core/framework/capture.h - the `shot` word: the D3D9 backbuffer written to a
// BMP under <data_dir>\shots, so the harness (and the session driving it) can
// SEE the game without a window screenshot, which is black for exclusive
// fullscreen. Runs on the present thread inside the Present hook; the readback
// is a GetRenderTargetData into a system-memory surface. BMPs are gitignored:
// game-derived content never enters the tree.
#pragma once

struct IDirect3DDevice9;

namespace d2vr::capture {
void request(const char* tag);            // from any thread; taken on the next present
void tick(IDirect3DDevice9* dev);         // present thread
void on_reset();                          // drop cached surfaces
unsigned long count();                    // shots written
const char* last_path();                  // "" until the first shot
} // namespace d2vr::capture
