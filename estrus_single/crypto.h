#pragma once
#include <Arduino.h>

String sha256(String input);
String hashPassword(String password, String salt);