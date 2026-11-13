// SPDX-License-Identifier: GPL-3.0-or-later
// The DualSense speaker/haptics path talks to the pad over SDL HID, which the PS4 build does not
// have. "No DualSense took it" is the documented answer for every call, so audio and rumble use
// their normal paths.
#include "libs/dualSenseHaptics.h"

namespace Libs::Controller::DualSenseHaptics {

Stream* Open(uint32_t, bool) { return nullptr; }
void    Close(Stream*) {}
bool    UsesBluetooth(const Stream*) { return false; }
uint64_t Queue(Stream*, int, const void*, uint32_t, uint32_t, bool, const int*, float, const float*) { return 0; }
bool    SetVibration(int, uint8_t, uint8_t, uint32_t) { return false; }
void    Shutdown() {}

} // namespace Libs::Controller::DualSenseHaptics
