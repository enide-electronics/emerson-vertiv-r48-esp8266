#ifndef NOKIA_DISPLAY_H
#define NOKIA_DISPLAY_H

#include <Arduino.h>
#include <IPAddress.h>

void nokiaDisplayBegin();
void nokiaDisplayAddAsLog(const char *line1, bool clearScreen = false);
void nokiaDisplayClear();
void nokiaDisplayNotifyCanMessage();
void nokiaDisplayUpdate(float outputVoltage,
                        float outputCurrent,
                        float outputCurrentLimit,
                        const char *networkName,
                        const IPAddress &ipAddress,
                        bool wifiOnline);

#endif