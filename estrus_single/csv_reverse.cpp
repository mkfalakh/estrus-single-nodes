#include "csv_reverse.h"

int readLastLines(File &file, char lines[][128], int maxLines) {

  int fileSize = file.size();
  if (fileSize <= 0) return 0;

  int lineCount = 0;
  int charIndex = 0;

  char buffer[128];
  memset(buffer, 0, sizeof(buffer));

  // mulai dari akhir file
  for (int pos = fileSize - 1; pos >= 0; pos--) {

    file.seek(pos);
    char c = file.read();

    if (c == '\n' || pos == 0) {

      // kalau awal file, tambahkan char terakhir
      if (pos == 0 && c != '\n') {
        buffer[charIndex++] = c;
      }

      // reverse buffer (karena kebalik)
      for (int i = 0; i < charIndex / 2; i++) {
        char tmp = buffer[i];
        buffer[i] = buffer[charIndex - i - 1];
        buffer[charIndex - i - 1] = tmp;
      }

      buffer[charIndex] = '\0';

      if (charIndex > 0) {
        strcpy(lines[lineCount], buffer);
        lineCount++;

        if (lineCount >= maxLines) break;
      }

      // reset buffer
      charIndex = 0;
      memset(buffer, 0, sizeof(buffer));

    } else {
      if (charIndex < 127) {
        buffer[charIndex++] = c;
      }
    }

    yield();  // 🔥 anti WDT
  }

  return lineCount;
}