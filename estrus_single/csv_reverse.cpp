#include "csv_reverse.h"
#include "config_runtime.h"
#include "sd_manager.h"

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

  constexpr int BUFFER_SIZE = 160;

  size_t fileSize = file.size();

  if (fileSize == 0) {
    return 0;
  }

  int startIndex = page * limit;
  int endIndex = startIndex + limit;

  int foundLines = 0;
  int copiedLines = 0;

  char buffer[BUFFER_SIZE];
  int charIndex = 0;

  constexpr size_t BLOCK_SIZE = 512;
  uint8_t block[BLOCK_SIZE];

  memset(buffer, 0, sizeof(buffer));

  // loop baru
  for (int32_t blockEnd = (int32_t)fileSize;
       blockEnd > 0;
       blockEnd -= BLOCK_SIZE) {

    int32_t blockStart =
      (blockEnd > (int32_t)BLOCK_SIZE)
        ? (blockEnd - (int32_t)BLOCK_SIZE)
        : 0;

    size_t bytesToRead =
      (size_t)(blockEnd - blockStart);

    file.seek(blockStart);

    size_t bytesRead =
      file.read(
        block,
        bytesToRead);

    if (bytesRead == 0) {
      continue;
    }

    for (int32_t i = bytesRead - 1;
         i >= 0;
         i--) {

      char c = (char)block[i];

      if (c == '\n' || (blockStart == 0 && i == 0)) {

        if (blockStart == 0 && i == 0 && c != '\n') {

          if (charIndex < BUFFER_SIZE - 1) {
            buffer[charIndex++] = c;
          }
        }

        if (charIndex > 0) {

          for (int j = 0;
               j < charIndex / 2;
               j++) {

            char tmp = buffer[j];

            buffer[j] =
              buffer[charIndex - 1 - j];

            buffer[charIndex - 1 - j] =
              tmp;
          }

          buffer[charIndex] = '\0';

          if (
            strncmp(
              buffer,
              "device_id,",
              10)
              != 0
            && csvLineMatchesDevice(
              buffer)) {

            if (
              foundLines >= startIndex
              && foundLines < endIndex
              && copiedLines < limit) {

              strncpy(
                lines[copiedLines],
                buffer,
                BUFFER_SIZE - 1);

              lines[copiedLines][BUFFER_SIZE - 1] =
                '\0';

              copiedLines++;
            }

            foundLines++;

            if (
              foundLines > endIndex) {

              hasNext = true;

              return copiedLines;
            }

            if (
              page == 0 && foundLines > limit) {

              hasNext = true;

              return copiedLines;
            }
          }
        }

        charIndex = 0;

      } else {

        if (
          c != '\r'
          && charIndex < BUFFER_SIZE - 1) {

          buffer[charIndex++] = c;
        }
      }
    }

    vTaskDelay(1);
  }

  // loop lama
  // for (int32_t pos = (int32_t)fileSize - 1;
  //      pos >= 0;
  //      pos--) {

  //   file.seek(pos);

  //   int ch = file.read();

  //   if (ch < 0) {
  //     continue;
  //   }

  //   char c = (char)ch;

  //   if (c == '\n' || pos == 0) {

  //     if (pos == 0 && c != '\n') {

  //       if (charIndex < BUFFER_SIZE - 1) {
  //         buffer[charIndex++] = c;
  //       }
  //     }

  //     if (charIndex > 0) {

  //       for (int i = 0; i < charIndex / 2; i++) {

  //         char tmp = buffer[i];

  //         buffer[i] = buffer[charIndex - 1 - i];

  //         buffer[charIndex - 1 - i] = tmp;
  //       }

  //       buffer[charIndex] = '\0';

  //       if (strncmp(buffer, "device_id,", 10) != 0
  //           && csvLineMatchesDevice(buffer)) {

  //         if (foundLines >= startIndex
  //             && foundLines < endIndex
  //             && copiedLines < limit) {

  //           strncpy(
  //             lines[copiedLines],
  //             buffer,
  //             BUFFER_SIZE - 1);

  //           lines[copiedLines][BUFFER_SIZE - 1] = '\0';

  //           copiedLines++;
  //         }

  //         foundLines++;

  //         if (foundLines > endIndex) {

  //           hasNext = true;

  //           break;
  //         }

  //         // page pertama cukup ambil limit+1 baris
  //         if (page == 0 && foundLines > limit) {

  //           hasNext = true;

  //           break;
  //         }
  //       }
  //     }

  //     charIndex = 0;

  //   } else {

  //     if (c != '\r' && charIndex < BUFFER_SIZE - 1) {

  //       buffer[charIndex++] = c;
  //     }
  //   }

  //   if ((pos & 0x3FF) == 0) {
  //     vTaskDelay(1);
  //   }
  // }

  return copiedLines;
}
