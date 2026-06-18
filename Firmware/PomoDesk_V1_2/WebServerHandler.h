#pragma once

#include <Arduino.h>

void startConfigServer();
void stopConfigServer();
void toggleConfigServer();
void processConfigServer();

void powerOffDevice();
void handleExitConfig();
void handleSleep();
