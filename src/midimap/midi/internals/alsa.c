#include "midi/internals/alsa.h"
#ifdef __linux__

/**
 * Stop Flag.
 * Flip to 1 to exit.
 */
static volatile sig_atomic_t stop = 0;

/**
 * Sets the stop flag to true,
 * which is monitored by a fd loop.
 */
static void sighandler() { stop = 1; }

static void update_node_list(mm_key_node** tail, int note_on, int midi);
static void check_snd(char* desc, int err);

mm_devices* mma_get_devices() {
    mm_devices* devices = malloc(sizeof(mm_devices));
    devices->store = malloc(sizeof(mm_device) * 64);
    devices->count = 0;

    snd_seq_t* seq = NULL;
    mma_init_sequencer(&seq, "device-query");

    snd_seq_client_info_t* cinfo;
    snd_seq_port_info_t* pinfo;

    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_client_info_set_client(cinfo, -1);

    while (snd_seq_query_next_client(seq, cinfo) >= 0) {

        int client_id = snd_seq_client_info_get_client(cinfo);
        /* printf("found client: %d\n", client_id); */

        snd_seq_port_info_set_client(pinfo, client_id);
        snd_seq_port_info_set_port(pinfo, -1);

        while (snd_seq_query_next_port(seq, pinfo) >= 0) {
            int port_id = snd_seq_port_info_get_port(pinfo);

            mm_device* dev = malloc(sizeof(mm_device));

            const char* pinfo_name = snd_seq_port_info_get_name(pinfo);
            dev->name = strdup(pinfo_name);
            // dev->name = pinfo_name;

            dev->client = client_id;
            dev->port = port_id;
            // dev->name = NULL;

            devices->store[devices->count] = dev;
            devices->count++;
        }
    }

    snd_seq_close(seq);
    snd_config_update_free_global();

    // downsize to accurate storage size.
    devices->store =
        realloc(devices->store, sizeof(mm_device) * devices->count);

    return devices;
}

bool mma_client_exists(char* client_with_port) {
    mm_device* dev = mm_parse_device(client_with_port);
    snd_seq_port_info_t* port = mma_get_port_info(dev);
    free(dev);

    bool exists = (port != NULL);
    snd_seq_port_info_free(port);

    return exists;
}

snd_seq_port_info_t* mma_get_port_info(mm_device* dev) {

    snd_seq_t* seq;
    mma_init_sequencer(&seq, NULL);

    snd_seq_port_info_t* pinfo;
    snd_seq_port_info_malloc(&pinfo);

    int found = snd_seq_get_any_port_info(seq, dev->client, dev->port, pinfo);

    snd_seq_close(seq);
    snd_config_update_free_global();

    if (found == 0) {
        return pinfo;

    } else {
        snd_seq_port_info_free(pinfo);
        mm_debug("Client(%d) port not found(%d)", dev->client, dev->port);
        return NULL;
    }
}

void mma_monitor_device(mm_options* options) {
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    mm_debug("alsa: mma_monitor_device: setting up src\n");
    mm_device* src = mm_parse_device(options->source);
    options->source = NULL;

    mm_debug("alsa: mma_monitor_device: attempting to query port_info\n");
    snd_seq_port_info_t* pinfo = mma_get_port_info(src);
    const char* pname = snd_seq_port_info_get_name(pinfo);

    printf("Monitoring: %s [%d:%d]\n\n", pname, src->client, src->port);
    mm_midi_output* output = mma_midi_output_create(src->client, src->port);

    if (options->target != NULL) {
        mm_debug("alsa: mma_monitor_device: setting up receiver\n");
        mm_device* receiver = mm_parse_device(options->target);
        options->target = NULL;
        mma_send_events_to(output, receiver->client, receiver->port);
        free(receiver);
    }

    mm_debug("alsa: mma_monitor_device: attempting to enter event loop\n");

    snd_seq_port_info_free(pinfo);
    free(src);

    mma_event_loop(options, output, &mm_monitor_render);
    mm_output_free(output);
}

void mma_event_loop(mm_options* options, mm_midi_output* output,
                    void (*render_callback)(mm_options* options,
                                            mm_key_node* tail,
                                            mm_key_set* key_set)) {

    int pfds_num =
        snd_seq_poll_descriptors_count(output->dev, POLLIN | POLLOUT);
    struct pollfd* pfds = malloc(pfds_num * sizeof(*pfds));

    mm_key_node* list = mm_key_node_head();
    mm_key_group* grp = NULL;
    mm_key_set* dsts_set = mm_create_key_set(0);

    snd_seq_event_t* event = NULL;

    int midi = 0;
    int type = 0;
    int chan = 0;
    int vel = 0;
    int note_on = 0;
    int err = 0;
    int process_event = 1;

    if (render_callback != NULL) {
        render_callback(options, list, dsts_set);
    }

    for (;;) {
        // gather poll descriptors for this sequencer
        snd_seq_poll_descriptors(output->dev, pfds, pfds_num, POLLIN);

        // wait on sequencer events
        if (poll(pfds, pfds_num, -1) < 0) {
            break;
        }

        // iterate over available events
        do {
            process_event = 1;
            event = NULL;
            grp = NULL;

            err = snd_seq_event_input(output->dev, &event);

            if (err < 0) {
                break;
            }

            if (event) {
                type = event->type;
                midi = event->data.note.note;
                chan = event->data.note.channel;
                vel = event->data.note.velocity;

                note_on = (type == SND_SEQ_EVENT_NOTEON) ? 1 : 0;

                update_node_list(&list, note_on, midi);

                process_event = mm_event_process(
                    output, options, list, &dsts_set, midi, chan, vel, note_on);

                if (process_event) {
                    // event->data.note.channel = 0;
                    mma_send_event(output, event);
                }
            }

        } while (err > 0);

        if (render_callback != NULL) {
            render_callback(options, list, dsts_set);
        }

        // Caught a sig signal, time to exit!
        if (stop) {
            break;
        }
    }

    // mm_output_free(output);
    mm_options_free(options);
    mm_keylets_free(dsts_set->keys, dsts_set->count);

    // free(dsts_set->keys);
    free(dsts_set);
    free(list);
    free(grp);
    free(pfds);
}

void mma_send_midi_note(int client, int port, char* note, bool on, int ch,
                        int vel) {
    mm_midi_output* output = mma_midi_output_create(client, port);
    mma_send_events_to(output, client, port);

    int midi = atoi(note);
    mma_send_midi(output, midi, on, ch, vel);

    mm_output_free(output);

    // TODO: close out_port since this is one-shot api
}

void mma_receive_events_from(mm_midi_output* output, int client, int port) {
    unsigned int port_caps = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE |
                             SND_SEQ_PORT_CAP_SUBS_READ |
                             SND_SEQ_PORT_CAP_SUBS_WRITE;

    unsigned int port_type =
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION |
        SND_SEQ_PORT_TYPE_MIDI_GM | SND_SEQ_PORT_TYPE_HARDWARE;

    int in_port =
        mma_create_port(output->dev, "mm-input", port_caps, port_type);

    output->in_ports[output->out_count] = in_port;
    output->in_count++;

    if (in_port < 0) {
        fprintf(stderr, "Error creating sequencer port.\n");
        exit(1);

    } else {
        int err = snd_seq_connect_from(output->dev, in_port, client, port);
        if (err < 0) {
            check_snd("snd_seq_connect_from", err);
        }
    }
}
void mma_send_events_to(mm_midi_output* output, int client, int port) {
    unsigned int port_caps = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE |
                             SND_SEQ_PORT_CAP_SUBS_READ |
                             SND_SEQ_PORT_CAP_SUBS_WRITE;

    unsigned int port_type =
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION |
        SND_SEQ_PORT_TYPE_MIDI_GM | SND_SEQ_PORT_TYPE_HARDWARE;

    int driver_port =
        mma_create_port(output->dev, "mm-output", port_caps, port_type);

    output->out_ports[output->out_count] = driver_port;
    output->out_count++;

    if (driver_port < 0) {
        fprintf(stderr, "Error creating sequencer port.\n");
        exit(1);

    } else {
        int err = snd_seq_connect_to(output->dev, driver_port, client, port);
        if (err < 0) {
            check_snd("snd_seq_connect_from", err);
        }
    }
}

mm_midi_output* mma_midi_output_create(int input_client, int input_port) {
    mm_midi_output* output = malloc(sizeof(mm_midi_output));
    output->dev = NULL;

    output->in_count = 0;
    output->out_count = 0;

    output->out_ports = malloc(sizeof(int*) * 64);
    output->in_ports = malloc(sizeof(int*) * 64);

    output->id = mma_init_sequencer(&output->dev, "midi-mapper");
    mma_receive_events_from(output, input_client, input_port);

    return output;
}

int mma_init_sequencer(snd_seq_t** seq, char* name) {
    int status;
    if ((status = snd_seq_open(seq, "default", SND_SEQ_OPEN_DUPLEX, 0)) < 0) {
        error("Could not open sequencer: %s", snd_strerror(status));
        exit(EXIT_FAILURE);
    }

    if (name != NULL) {
        snd_seq_set_client_name(*seq, name);
    }

    snd_seq_nonblock(*seq, 1);

    int id = snd_seq_client_id(*seq);

    // free cached memory (in regards to valgrind)
    snd_config_update_free_global();

    return id;
}

int mma_create_port(snd_seq_t* seq, char* name, unsigned caps, unsigned type) {
    int port_result = snd_seq_create_simple_port(seq, name, caps, type);

    return port_result;
}

void mma_send_midi(mm_midi_output* output, int midi, bool on, int ch, int vel) {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);

    ev.type = on == true ? SND_SEQ_EVENT_NOTEON : SND_SEQ_EVENT_NOTEOFF;
    ev.data.control.channel = ch;
    ev.data.note.note = midi;
    ev.data.note.velocity = vel;

    mma_send_event(output, &ev);
}

void mma_send_event(mm_midi_output* output, snd_seq_event_t* ev) {

    // publish to any subscribers to the sequencer
    snd_seq_ev_set_subs(ev);

    // set output to direct
    snd_seq_ev_set_direct(ev);

    int port_count = 0;
    int* ports = NULL;

    if (output->out_count == 0) {
        port_count = output->in_count;
        ports = output->in_ports;
    } else {
        port_count = output->out_count;
        ports = output->out_ports;
    }

    for (int i = 0; i < port_count; ++i) {
        /* printf("\nsend_event[%d] -> %d\n", ports[i], ev->data.note.note); */

        // set event source
        snd_seq_ev_set_source(ev, ports[i]);

        // output event immediately
        snd_seq_event_output_direct(output->dev, ev);
    }

    snd_seq_free_event(ev);
}

static void update_node_list(mm_key_node** tail, int note_on, int midi) {
    mm_key_node* node = NULL;

    if (note_on) {
        node = NULL;
        node = mm_key_node_search(tail, midi);

        if (node == NULL) {
            // Create the new node and set it up
            node = mm_key_node_create(midi);

            // Insert the new into the list by adjoining with the tail.
            mm_key_node_insert(tail, node);
        }

    } else {
        node = mm_key_node_search(tail, midi);

        if (node != NULL) {
            mm_key_node_remove(tail, node);
        }
    }
}

static void check_snd(char* desc, int err) {
    if (err < 0) {
        error("Alsa Subsystem error: failed to %s because %s", desc,
              snd_strerror(err));
        exit(EXIT_FAILURE);
    }
}
#endif
