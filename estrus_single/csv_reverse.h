#pragma once
#include <Arduino.h>
#include <SD.h>

int readCsvPage(File &file,
                char (*lines)[128],
                int page,
                int limit,
                bool &hasNext);