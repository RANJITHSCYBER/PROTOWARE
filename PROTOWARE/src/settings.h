#ifndef PROTOWARE_SETTINGS_H
#define PROTOWARE_SETTINGS_H

#include <Arduino.h>
#include "display.h"

void initSettings();
void saveAppearanceConfig();
void applyCurrentColorScheme();
void resetActivityTimer();
void checkStandbyTimeout();

void enterSettingsSubmenu();
void handleSettingsMenu();

extern byte colorSelectionIndex;
extern byte standbyTimeoutIndex;

#endif // PROTOWARE_SETTINGS_H

