#pragma once
#include <Arduino.h>
#include <SD.h>

int readLastLines(File &file, char lines[][128], int maxLines);