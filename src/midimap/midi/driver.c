#include "driver.h"

mm_devices* mm_get_devices() {
#ifdef __APPLE__
    return mmc_get_devices();
#elif __linux__
    return mma_get_devices();
#endif
}

bool mm_client_exists(char* client) {
#ifdef __APPLE__
#TODO : implement.
#elif __linux__
    return mma_client_exists(client);
#endif
}

void mm_monitor_client(mm_options* options) {
#ifdef __APPLE__
// TODO : implement.
#elif __linux__
    mma_monitor_device(options);
#endif
}

void mm_event_loop(mm_options* options, mm_midi_output* output,
                   void (*render_callback)(mm_options* options,
                                           mm_key_node* tail,
                                           mm_key_set* key_set)) {
#ifdef __APPLE__
// TODO : implement.
#elif __linux__
    mma_event_loop(options, output, render_callback);
#endif
}

void mm_list_clients() {
#ifdef __linux__
    mm_devices* devices = mma_get_devices();
#elif __APPLE__
// TODO: implement
#endif
    if (devices->count > 0) {
        printf("Device Ports found (%d):\n", devices->count);
    } else {
        printf("No MIDI Device Ports found to be available.\n\n");
    }

    for (int i = 0; i < devices->count; ++i) {
        mm_device* device = devices->store[i];
        char id[12];
        snprintf(id, sizeof(char) * 12, "[%d:%d]", device->client,
                 device->port);

        if (device->name != NULL) {
            printf("%-8s %s\n", id, device->name);
        } else {
            printf("%-8s\n", id);
        }
    }

    printf("\n");
}

mm_device* mm_get_midi_through() {
    mm_devices* devices = mm_get_devices();
    int size = sizeof(char*) * 64;

    for (int i = 0; i < devices->count; ++i) {
        if (devices->store[i]->name != NULL) {
            // char* n_dup = strdup(n);
            // for (int is = 0; n_dup[is]; ++is) {
            //     n_dup[is] = tolower(n_dup[is]);
            // }
            char* name = malloc(size);
            snprintf(name, size, "%s", devices->store[i]->name);

            char* match = strstr(name, "Midi Through");
            free(name);

            if (match != NULL) {
                // mm_debug("mm_get_midi_through: found it: %s\n",
                //          devices->store[i]->name);
                int client = devices->store[i]->client;
                int port = devices->store[i]->port;

                mm_devices_free(devices);
                mm_device* dev = malloc(sizeof(mm_device));

                dev->client = client;
                dev->port = port;
                return dev;
            } else {
                // printf("mm_get_midi_through(): device not matched: %s\n",
                //        devices->store[i]->name);
                mm_debug("mm_get_midi_through: not a match: %s\n",
                         devices->store[i]->name);
            }
        }
    }

    mm_debug("mm_get_midi_through: not found\n");
    mm_devices_free(devices);
    return NULL;
}

void mm_send_midi_to_client(int client, int port, char* note, bool on,
                            int channel, int vel) {
    printf("mm_send_midi_to_client -> %d:%d [%s, %d, ch:%d, vel:%d]", client,
           port, note, on, channel, vel);
#ifdef __linux__
    mma_send_midi_note(client, port, note, on, channel, vel);
#elif __APPLE__
// TODO:
#endif
}

void mm_send_midi(mm_midi_output* output, int midi, bool on, int ch, int vel) {
#ifdef __linux__
    mma_send_midi(output, midi, on, ch, vel);
#elif __APPLE__
// TODO:
#endif
}
void mm_send_event(mm_midi_output* output, mm_midi_event* ev) {
#ifdef __linux__
    mma_send_event(output, ev);
#elif __APPLE__
// TODO:
#endif
}

void mm_driver_init(mm_midi_device** dev, char* name) {
#ifdef __linux__
    mma_init_sequencer(dev, name);
#elif __APPLE__
// TODO:
#endif
}

void mm_send_events_to(mm_midi_output* output, int client, int port) {
#ifdef __linux__
    mma_send_events_to(output, client, port);
#elif __APPLE__
// TODO:
#endif
}

void mm_receive_events_from(mm_midi_output* output, int client, int port) {
#ifdef __linux__
    mma_receive_events_from(output, client, port);
#elif __APPLE__
// TODO:
#endif
}

void mm_driver_debug() { mm_driver_debug_mode = true; }
