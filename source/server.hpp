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

#pragma once

#include <string>
#include <switch.h>

int setupServerSocket();
std::string buildJSONPayload(u64 keys, JoystickPosition lPos, JoystickPosition rPos);
