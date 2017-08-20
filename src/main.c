#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "midi/driver.h"
#include "ui/interface.h"

int main(int argc, char **argv) {

  if (argc > 2 || argc < 2) {
    printf("Usage:\n  %s <midi device name>\n", argv[0]);
    return 1;
  }

  Devices *devices = MM_GetDevices();

  if (devices->count > 0) {
    printf("midimapper found devices:\n");

    for (int i = 0; i < devices->count; ++i) {
      printf(" %02d: %s\n", i, devices->store[i]->name);
    }

    MM_AttachListener(devices->store[0], &MM_MIDIReadProc);

    // Kick off the application UI thread
    MMInterfaceStart();

  } else {
    printf("midimapper: no midi devices available\n");
  }

  return 0;
}
