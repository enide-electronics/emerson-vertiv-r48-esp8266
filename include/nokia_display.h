#ifndef NOKIA_DISPLAY_H
#define NOKIA_DISPLAY_H

#include <Arduino.h>
#include <IPAddress.h>

void nokiaDisplayBegin();
void nokiaDisplayShowStatus(const char *line1, const char *line2);
void nokiaDisplayClear();
void nokiaDisplayUpdate(float outputVoltage,
                        float outputCurrent,
                        float outputCurrentLimit,
                        const char *networkName,
                        const IPAddress &ipAddress);

#endif