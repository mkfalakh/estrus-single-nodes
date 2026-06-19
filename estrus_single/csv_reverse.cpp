#include "csv_reverse.h"
#include "config_runtime.h"

// Returns true if columns 0 (device_id) and 1 (animal_id) match sysConfig.
static bool csvLineMatchesDevice(const char *line) {
  const char *p = line;
  const char *d0 = p;
  while (*p && *p != ',') p++;
  int d0len = p - d0;
  if (!*p) return false;
  p++;
  const char *d1 = p;
  while (*p && *p != ',') p++;
  int d1len = p - d1;

  int nlen = strlen(sysConfig.node_id);
  int alen = strlen(sysConfig.animal_id);

  if (nlen > 0 && (d0len != nlen || strncmp(d0, sysConfig.node_id, nlen) != 0)) return false;
  if (alen > 0 && (d1len != alen || strncmp(d1, sysConfig.animal_id, alen) != 0)) return false;
  return true;
}

int readCsvPage(File &file,
                char (*lines)[160],
                int page,
                int limit,
                bool &hasNext) {

  hasNext = false;

  if (!file || limit <= 0 || page < 0) {
    return 0;
  }

  const int BUFFER_SIZE = 160;

  int fileSize = file.size();

  if (fileSize <= 0) {
    return 0;
  }

  int startIndex = page * limit;
  int endIndex = startIndex + limit;

  int foundLines = 0;
  int copiedLines = 0;

  char buffer[BUFFER_SIZE];
  int charIndex = 0;

  memset(buffer, 0, sizeof(buffer));

  // baca dari akhir file
  for (int pos = fileSize - 1; pos >= 0; pos--) {

    file.seek(pos);

    char c = file.read();

    // akhir baris ditemukan
    if (c == '\n' || pos == 0) {

      // jika awal file, masukkan karakter terakhir
      if (pos == 0 && c != '\n') {

        if (charIndex < BUFFER_SIZE - 1) {
          buffer[charIndex++] = c;
        }
      }

      if (charIndex > 0) {

        // reverse buffer
        for (int i = 0; i < charIndex / 2; i++) {

          char tmp = buffer[i];

          buffer[i] = buffer[charIndex - 1 - i];

          buffer[charIndex - 1 - i] = tmp;
        }

        buffer[charIndex] = '\0';

        // skip header CSV and records not belonging to this device/animal
        if (strncmp(buffer, "device_id,", 10) != 0
            && csvLineMatchesDevice(buffer)) {

          // sudah masuk halaman yang diminta
          if (foundLines >= startIndex && foundLines < endIndex) {

            strncpy(lines[copiedLines],
                    buffer,
                    BUFFER_SIZE - 1);

            lines[copiedLines][BUFFER_SIZE - 1] = '\0';

            copiedLines++;
          }

          foundLines++;

          // cek apakah masih ada data setelah halaman ini
          if (foundLines > endIndex) {

            hasNext = true;

            break;
          }
        }
      }

      // reset buffer
      charIndex = 0;

      memset(buffer, 0, sizeof(buffer));
    } else {

      // simpan karakter jika buffer belum penuh
      if (c != '\r' && charIndex < BUFFER_SIZE - 1) {

        buffer[charIndex++] = c;
      }
    }

    static uint16_t watchdogCounter = 0;

    if (++watchdogCounter >= 256) {

      watchdogCounter = 0;

      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  return copiedLines;
}
