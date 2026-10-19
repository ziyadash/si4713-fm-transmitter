#include "config.h"

#if BRINGUP_MODE
#include "src/bringup/bringup.h"
void setup() { bringupSetup(); }
void loop() { bringupLoop(); }
#else
#include "src/app/app.h"
void setup() { appSetup(); }
void loop() { appLoop(); }
#endif
