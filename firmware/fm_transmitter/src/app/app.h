#pragma once

// The full firmware (Bluetooth sink, LCD, radio). Compiled only when
// BRINGUP_MODE is 0 in config.h.
void appSetup();
void appLoop();
