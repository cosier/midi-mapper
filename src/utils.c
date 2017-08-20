#include "utils.h"

#ifdef __APPLE__
char *CFStringRefToChars(CFStringRef string) {
  if (string == NULL) {
    return NULL;
  }

  CFIndex length = CFStringGetLength(string);
  CFIndex maxSize =
      CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;

  // Allocate memory
  char *buffer = (char *)malloc(maxSize);

  // Attempt conversion into the buffer
  if (CFStringGetCString(string, buffer, maxSize, kCFStringEncodingUTF8)) {
    return buffer;
  }

  // Could not convert CFStringRef, free then return NULL.
  free(buffer);
  return NULL;
}

CFStringRef CharToCFStringRef(char *c) {
  return CFStringCreateWithCStringNoCopy(NULL, c, kCFStringEncodingUTF8, NULL);
}

#endif