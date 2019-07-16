/*
 * Open Joystick Display Server NX
 * Copyright (C) 2019 Steven Mattera
 * This file is part of OJDS-NX <https://github.com/chiditarod/dogtag>.
 *
 * OJDS-NX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OJDS-NX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OJDS-NX. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <switch.h>
#include <jansson.h>
#include <limits.h>

#define INNER_HEAP_SIZE 0x1EFB7
#define MAX_LINE_LENGTH 26
#define SOCK_BUFFERSIZE 256

u32 __nx_applet_type = AppletType_None;

size_t nx_inner_heap_size = INNER_HEAP_SIZE;
char   nx_inner_heap[INNER_HEAP_SIZE];

void __libnx_initheap(void) {
	void*  addr = nx_inner_heap;
	size_t size = nx_inner_heap_size;

	extern char * fake_heap_start;
	extern char * fake_heap_end;

	fake_heap_start = (char *)addr;
	fake_heap_end   = (char *)addr + size;
}

void __appInit(void) {
    Result rc;

    rc = smInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    rc = hidInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));

    static const SocketInitConfig socketInitConfig = {
        .bsdsockets_version = 1,

        .tcp_tx_buf_size = 1024,
        .tcp_rx_buf_size = 256,
        .tcp_tx_buf_max_size = 0,
        .tcp_rx_buf_max_size = 0,

        .udp_tx_buf_size = 0x2400,
        .udp_rx_buf_size = 0xA500,

        .sb_efficiency = 2,
    };

    rc = socketInitialize(&socketInitConfig);
    if (R_FAILED(rc))
        fatalSimple(rc);
}

void __appExit(void) {
    socketExit();
    hidExit();
    smExit();
}

// Copied from sys-netcheat. Thanks Jakibaki!
int setupServerSocket() {
    int lissock;
    struct sockaddr_in server;
    lissock = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(56709);
    while (bind(lissock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        svcSleepThread(1e+9L);
    }

    listen(lissock, 3);
    return lissock;
}

// Builds the Chromium Joystick model to be sent back to OJD.
char * buildJSONPayload(u64 keys, JoystickPosition lPos, JoystickPosition rPos) {
    json_t * root = json_object();
    json_t * axes = json_array();
    json_t * buttons = json_array();

    json_object_set_new(root, "axes", axes);
    json_object_set_new(root, "buttons", buttons);
    json_object_set_new(root, "connected", json_true());
    json_object_set_new(root, "id", json_string("Nintendo Switch"));
    json_object_set_new(root, "index", json_integer(0));
    json_object_set_new(root, "mapping", json_string("standard"));
    json_object_set_new(root, "timestamp", json_real(0));

    json_array_append_new(axes, json_real((double) lPos.dx / (double) SHRT_MAX));
    json_array_append_new(axes, json_real((double) lPos.dy * -1 / (double) SHRT_MAX));
    json_array_append_new(axes, json_real((double) rPos.dx / (double) SHRT_MAX));
    json_array_append_new(axes, json_real((double) rPos.dy * -1 / (double) SHRT_MAX));

    for (int i = 0; i < 16; i++)
    {
        bool keyPressed = keys & BIT(i);
        json_t * button = json_object();
        json_object_set_new(button, "pressed", json_boolean(keyPressed));
        json_object_set_new(button, "value", json_integer((keyPressed) ? 1 : 0));
        json_array_append_new(buttons, button);
    }

    char * json = json_dumps(root, JSON_COMPACT);
    size_t jsonLength = strlen(json);

    json_decref(root);

    // Build the JSON Socket packet.
    char * result = malloc(sizeof(char) * jsonLength + 5);
    sprintf(result, "%ld#%s", jsonLength, json);
    free(json);

    return result;
}

int main(int argc, char * argv[]) {
    char * linebuf = (char *) malloc(sizeof(char) * MAX_LINE_LENGTH);
    int listenfd = setupServerSocket();
    int sock = -1;
    int c = sizeof(struct sockaddr_in);
    struct sockaddr_in client;

    while (appletMainLoop()) {
        // More code copied from sys-netcheat.
        sock = accept(listenfd, (struct sockaddr *)&client, (socklen_t *)&c);
        if (sock <= 0) {
            svcSleepThread(1e+9L);
            close(listenfd);
            listenfd = setupServerSocket();
            continue;
        }

        while (1) {
            // Wait for OJD to poll for controller inputs.
            int len = recv(sock, linebuf, MAX_LINE_LENGTH, 0);
            if (len < 1) {
                break;
            }
            
            hidScanInput();
            u64 keys = hidKeysHeld(CONTROLLER_P1_AUTO);

            JoystickPosition lPos;
            JoystickPosition rPos;
            hidJoystickRead(&lPos, CONTROLLER_P1_AUTO, JOYSTICK_LEFT);
            hidJoystickRead(&rPos, CONTROLLER_P1_AUTO, JOYSTICK_RIGHT);

            char * payload = buildJSONPayload(keys, lPos, rPos);
            write(sock, payload, strlen(payload));
            free(payload);
        }
    }

    free(linebuf);
    return 0;
}