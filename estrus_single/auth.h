#pragma once
#include <Arduino.h>

bool createSession(String &sessionId);
bool validateSession(const String &sessionId);
void cleanupSessions();
String getSessionFromHeader(const String &cookie);