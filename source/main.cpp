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

#include <switch.h>
#include <sys/socket.h>

#include "server.hpp"

#define INNER_HEAP_SIZE 0x1EFB7
#define BUFFER_SIZE 26

extern "C" {
    extern u32 __start__;

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
}

int main(int argc, char * argv[]) {
    auto buffer = new char[BUFFER_SIZE];
    auto server_sock = setupServerSocket();
    int client_sock = -1;

    while (appletMainLoop()) {
        client_sock = accept(server_sock, NULL, NULL);
        if (client_sock <= 0) {
            svcSleepThread(1e+9L);
            close(server_sock);
            server_sock = setupServerSocket();
            continue;
        }

        while (1) {
            // Wait for OJD to poll for controller inputs.
            auto len = recv(client_sock, buffer, BUFFER_SIZE, 0);
            if (len < 1) {
                close(client_sock);
                break;
            }
            
            hidScanInput();
            auto keys = hidKeysHeld(CONTROLLER_P1_AUTO);

            JoystickPosition lPos;
            JoystickPosition rPos;
            hidJoystickRead(&lPos, CONTROLLER_P1_AUTO, JOYSTICK_LEFT);
            hidJoystickRead(&rPos, CONTROLLER_P1_AUTO, JOYSTICK_RIGHT);

            auto payload = buildJSONPayload(keys, lPos, rPos);
            write(client_sock, payload.c_str(), payload.size());
        }
    }

    if (client_sock > 0) {
        close(client_sock);
    }
    close(server_sock);
    delete buffer;
    return 0;
}
