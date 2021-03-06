/*
 * Astra Module: DVB-ASI
 * http://cesbo.com/astra
 *
 * Copyright (C) 2012-2014, Andrey Dyldin <and@cesbo.com>
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

/*
 * Module Name:
 *      asi_input
 *
 * Module Role:
 *      Source, demux endpoint
 */

#include <astra/astra.h>
#include <astra/core/event.h>
#include <astra/luaapi/stream.h>

#include <sys/ioctl.h>

#define MSG(_msg) "[asi_input %d] " _msg, mod->adapter

#define ASI_BUFFER_SIZE (1022 * TS_PACKET_SIZE)
#define ASI_IOC_RXSETPF _IOW('?', 76, unsigned int [256])

struct module_data_t
{
    STREAM_MODULE_DATA();

    int adapter;
    bool budget;

    int fd;
    asc_event_t *event;
    uint8_t filter[TS_MAX_PIDS / 8];

    uint8_t buffer[ASI_BUFFER_SIZE];
};

static void asi_on_error(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    asc_log_error(MSG("asi read error [%s]"), strerror(errno));
    asc_event_close(mod->event);
    close(mod->fd);
    asc_lib_abort();
}


static void asi_on_read(void *arg)
{
    module_data_t *mod = (module_data_t *)arg;

    const ssize_t len = read(mod->fd, mod->buffer, ASI_BUFFER_SIZE);
    if(len <= 0)
    {
        asi_on_error(mod);
        return;
    }

    for(int i = 0; i < len; i += TS_PACKET_SIZE)
        module_stream_send(mod, &mod->buffer[i]);
}


static void set_pid(module_data_t *mod, uint16_t pid, int is_set)
{
    if(mod->budget)
        return;

    if(!ts_pid_valid(pid))
    {
        asc_log_error(MSG("PID value must be less then %d"), TS_MAX_PIDS);
        asc_lib_abort();
    }

    if(is_set)
        mod->filter[pid / 8] |= (0x01 << (pid % 8));
    else
        mod->filter[pid / 8] &= ~(0x01 << (pid % 8));

    if(ioctl(mod->fd, ASI_IOC_RXSETPF, mod->filter) < 0)
    {
        asc_log_error(MSG("failed to set PES filter [%s]"), strerror(errno));
    }
}

static void join_pid(module_data_t *mod, uint16_t pid)
{
    if (!module_demux_check(mod, pid))
        set_pid(mod, pid, 1);

    module_demux_join(mod, pid);
}

static void leave_pid(module_data_t *mod, uint16_t pid)
{
    module_demux_leave(mod, pid);

    if (!module_demux_check(mod, pid))
        set_pid(mod, pid, 0);
}

static void module_init(lua_State *L, module_data_t *mod)
{
    module_stream_init(L, mod, NULL);
    module_demux_set(mod, join_pid, leave_pid);

    if(!module_option_integer(L, "adapter", &mod->adapter))
    {
        asc_log_error("[asi_input] option 'adapter' is required");
        asc_lib_abort();
    }
    module_option_boolean(L, "budget", &mod->budget);

    char dev_name[32];
    snprintf(dev_name, sizeof(dev_name), "/dev/asirx%d", mod->adapter);

    mod->fd = open(dev_name, O_RDONLY);
    if(mod->fd <= 0)
    {
        asc_log_error(MSG("failed to open device %s [%s]")
                      , dev_name, strerror(errno));

        mod->fd = 0;
        return;
    }

    if(!mod->budget)
    { /* hw filtering */
        memset(mod->filter, 0x00, sizeof(mod->filter));
    }
    else
    {
        memset(mod->filter, 0xFF, sizeof(mod->filter));
        mod->filter[TS_NULL_PID / 8] &= ~(0x01 << (TS_NULL_PID % 8));
    }

    if(ioctl(mod->fd, ASI_IOC_RXSETPF, mod->filter) < 0)
        asc_log_error(MSG("failed to set PES filter [%s]"), strerror(errno));

    fsync(mod->fd);

    mod->event = asc_event_init(mod->fd, mod);
    asc_event_set_on_read(mod->event, asi_on_read);
    asc_event_set_on_error(mod->event, asi_on_error);
}

static void module_destroy(module_data_t *mod)
{
    module_stream_destroy(mod);

    ASC_FREE(mod->event, asc_event_close);
    if(mod->fd)
        close(mod->fd);
}

STREAM_MODULE_REGISTER(asi_input)
{
    .init = module_init,
    .destroy = module_destroy,
};
