#include "csv_reverse.h"

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

        // skip header CSV
        if (strncmp(buffer,
                    "device_id,",
                    10)
            != 0) {

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
