/*
 * Astra Module: Pipe
 *
 * Copyright (C) 2015, Artem Kharitonov <artem@sysert.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PIPE_H_
#define _PIPE_H_ 1

#include <astra.h>
#include <core/stream.h>
#include <core/timer.h>

#ifndef _REMOVE_ME_
    /*
     * TODO: move `child' to core
     */
#   include "child.h"
#endif /* _REMOVE_ME_ */

struct module_data_t
{
    MODULE_STREAM_DATA();

    char name[128];
    asc_timer_t *restart;
    unsigned delay;

    asc_child_cfg_t config;
    asc_child_t *child;
};

#endif /* _PIPE_H_ */
