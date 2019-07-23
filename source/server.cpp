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
 
#include <arpa/inet.h>
#include <jansson.h>
#include <limits.h>
#include <sys/socket.h>

#include "server.hpp"

using namespace std;

int setupServerSocket() {
    auto server_sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(56709);

    auto addr = (struct sockaddr *) &server;
    while (bind(server_sock, addr, sizeof(server)) < 0) {
        svcSleepThread(1e+9L);
    }

    listen(server_sock, 3);

    return server_sock;
}

string buildJSONPayload(u64 keys, JoystickPosition lPos, JoystickPosition rPos) {
    auto root = json_object();
    auto axes = json_array();
    auto buttons = json_array();

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

    for (int i = 0; i < 16; i++) {
        bool keyPressed = keys & BIT(i);
        auto button = json_object();
        json_object_set_new(button, "pressed", json_boolean(keyPressed));
        json_object_set_new(button, "value", json_integer((keyPressed) ? 1 : 0));
        json_array_append_new(buttons, button);
    }

    auto json = json_dumps(root, JSON_COMPACT);
    auto result = string(json);

    free(json);
    json_decref(root);

    result.insert(0, to_string(result.size()) + "#");

    return result;
}
