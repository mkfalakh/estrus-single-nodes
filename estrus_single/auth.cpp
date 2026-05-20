#include "auth.h"
#include "logger.h"

#define MAX_SESSIONS 5
// #define SESSION_TIMEOUT 1800000  // 30 menit
#define SESSION_TIMEOUT 3600000  // 60 menit

struct Session {
  String id;
  unsigned long lastAccess;
  bool active;
};

Session sessions[MAX_SESSIONS];

String generateSessionId() {
  String id = "";
  for (int i = 0; i < 16; i++) {
    id += String(random(0, 16), HEX);
  }
  return id;
}

bool createSession(String &sessionId) {
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (!sessions[i].active) {
      sessionId = generateSessionId();
      sessions[i] = { sessionId, millis(), true };
      return true;
    }
  }
  return false;
}

bool validateSession(const String &sessionId) {
  logToFile("🔍 Validate: %s", sessionId.c_str());

  unsigned long now = millis();

  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (sessions[i].active) {
      // logToFile("➡️ Compare with: " + sessions[i].id);
      logToFile("➡️ Compare with: %s", sessions[i].id.c_str());
    }

    if (sessions[i].active && sessions[i].id == sessionId) {

      if (now - sessions[i].lastAccess > SESSION_TIMEOUT) {
        sessions[i].active = false;
        return false;
      }

      sessions[i].lastAccess = now;
      logToFile("✅ Session VALID\n");
      return true;
    }
  }

  logToFile("❌ Session NOT FOUND");
  return false;
}

void cleanupSessions() {
  unsigned long now = millis();
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (sessions[i].active && now - sessions[i].lastAccess > SESSION_TIMEOUT) {
      sessions[i].active = false;
    }
  }
}

String getSessionFromHeader(const String &cookie) {
  int start = cookie.indexOf("ESPSESSIONID=");
  if (start == -1) return "";

  start += strlen("ESPSESSIONID=");

  int end = cookie.indexOf(';', start);
  if (end == -1) {
    end = cookie.length();  // 🔥 FIX penting
  }

  String sessionId = cookie.substring(start, end);
  sessionId.trim();  // 🔥 bersihin spasi

  return sessionId;
}