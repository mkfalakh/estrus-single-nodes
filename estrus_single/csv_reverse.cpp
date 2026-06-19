#include "csv_reverse.h"
#include "config_runtime.h"

// Byte-by-byte field match — avoids ROM strncmp which uses L32I word loads
// and crashes with LoadStoreAlignment when the source pointer is not 4-byte
// aligned (d1 = line + node_id_len+1 can be at any alignment).
static bool csvLineMatchesDevice(const char *line) {
  int nlen = strlen(sysConfig.node_id);
  int alen = strlen(sysConfig.animal_id);

  if (nlen == 0 && alen == 0) return true;

  const char *p = line;

  // --- field 0: device_id ---
  if (nlen > 0) {
    for (int i = 0; i < nlen; i++, p++) {
      if (*p != sysConfig.node_id[i]) return false;
    }
    if (*p != ',') return false;
    p++;
  } else {
    while (*p && *p != ',') p++;
    if (!*p) return false;
    p++;
  }

  // --- field 1: animal_id ---
  if (alen > 0) {
    for (int i = 0; i < alen; i++, p++) {
      if (*p != sysConfig.animal_id[i]) return false;
    }
    if (*p != ',' && *p != '\0') return false;
  }

  return true;
}

// Reads up to `limit` data rows starting at reverse-order index `page*limit`
// (newest row = index 0) by scanning the file backwards in 512-byte chunks.
//
// Old approach: one file.seek()+file.read() per byte — 100k+ SD ops for a
// typical day file, holding the SD mutex for seconds.
// New approach: ~200 chunk reads for the same file, mutex held for ~100 ms.
int readCsvPage(File &file,
                char (*lines)[160],
                int page,
                int limit,
                bool &hasNext) {

  hasNext = false;

  if (!file || limit <= 0 || page < 0) return 0;

  const int CHUNK   = 512;
  const int LINE_MAX = 159;

  int fileSize = file.size();
  if (fileSize <= 0) return 0;

  int startIdx = page * limit;
  int endIdx   = startIdx + limit;

  // rev accumulates the current line's characters in REVERSE order as we
  // scan the file backwards; we flip in-place when a '\n' is found.
  char rev[LINE_MAX + 1] __attribute__((aligned(4)));
  int  revLen = 0;

  int foundLines  = 0;
  int copiedLines = 0;

  uint8_t chunk[CHUNK];
  int filePos = fileSize;

  while (filePos > 0) {

    int readSize = (filePos >= CHUNK) ? CHUNK : filePos;
    filePos -= readSize;

    file.seek(filePos);
    file.read(chunk, readSize);

    for (int i = readSize - 1; i >= 0; i--) {
      char c = (char)chunk[i];

      if (c == '\n') {

        if (revLen > 0) {
          // reverse in-place → forward line
          for (int l = 0, r = revLen - 1; l < r; l++, r--) {
            char t = rev[l]; rev[l] = rev[r]; rev[r] = t;
          }
          rev[revLen] = '\0';

          // skip CSV header (byte-by-byte, no strncmp)
          bool isHeader = (revLen >= 9
            && rev[0]=='d' && rev[1]=='e' && rev[2]=='v'
            && rev[3]=='i' && rev[4]=='c' && rev[5]=='e'
            && rev[6]=='_' && rev[7]=='i' && rev[8]=='d');

          if (!isHeader && csvLineMatchesDevice(rev)) {

            if (foundLines >= startIdx && foundLines < endIdx) {
              strncpy(lines[copiedLines], rev, LINE_MAX);
              lines[copiedLines][LINE_MAX] = '\0';
              copiedLines++;
            }

            foundLines++;

            if (foundLines > endIdx) {
              hasNext = true;
              return copiedLines;
            }
          }

          revLen = 0;
        }

      } else if (c != '\r') {
        if (revLen < LINE_MAX) rev[revLen++] = c;
      }
    }

    taskYIELD();
  }

  // flush the very first line (start of file, no leading '\n')
  if (revLen > 0) {
    for (int l = 0, r = revLen - 1; l < r; l++, r--) {
      char t = rev[l]; rev[l] = rev[r]; rev[r] = t;
    }
    rev[revLen] = '\0';

    bool isHeader = (revLen >= 9
      && rev[0]=='d' && rev[1]=='e' && rev[2]=='v'
      && rev[3]=='i' && rev[4]=='c' && rev[5]=='e'
      && rev[6]=='_' && rev[7]=='i' && rev[8]=='d');

    if (!isHeader && csvLineMatchesDevice(rev)) {

      if (foundLines >= startIdx && foundLines < endIdx) {
        strncpy(lines[copiedLines], rev, LINE_MAX);
        lines[copiedLines][LINE_MAX] = '\0';
        copiedLines++;
      }

      foundLines++;
      if (foundLines > endIdx) hasNext = true;
    }
  }

  return copiedLines;
}
