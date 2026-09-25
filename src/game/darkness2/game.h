// game/darkness2/game.h - the game side of the framework: the build fingerprint,
// the seam's game words, the status provider, the present tick, the canaries.
#pragma once
#include <stdint.h>

namespace d2vr::game {

struct Fingerprint {
    bool     checked = false;
    bool     ok = false;            // every field matched patterns.h
    uint32_t timeDateStamp = 0;
    uint32_t sizeOfImage = 0;
    uint64_t fileSize = 0;
};
const Fingerprint& fingerprint();   // reads the mapped exe once; logs the comparison
bool code_hooks_allowed();          // fingerprint ok AND not disabled

void init();                        // from the first Direct3DCreate9(Ex); registers everything

} // namespace d2vr::game
