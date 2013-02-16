/* Copyright (C) 2011 ~ 2012 Deepin, Inc.
 *               2011 ~ 2012 Long Changjin
 * 
 * Author:     Long Changjin <admin@longchangjin.cn>
 * Maintainer: Long Changjin <admin@longchangjin.cn>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <pulse/pulseaudio.h>

void pa_state_cb(pa_context *c, void *userdate);
void pa_server_info_cb(pa_context *c, const pa_server_info *i, void *userdate);

int main(int argc, const char *argv[])
{
    pa_mainloop *pa_ml = NULL;
    pa_mainloop_api *pa_mlapi = NULL;
    pa_operation *pa_op = NULL;
    pa_context *pa_ctx = NULL;

    int pa_ready = 0;
    int state = 0;
    int i = 0;

    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, "deepin");
    
    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, pa_state_cb, &pa_ready);

    for(;;) {
        if (pa_ready == 0) {
            pa_mainloop_iterate(pa_ml, 1, NULL);
            continue;
        }
        if (pa_ready == 2) {
            pa_context_disconnect(pa_ctx);
            pa_context_unref(pa_ctx);
            pa_mainloop_free(pa_ml);
            return -1;
        }
        switch (state) {
            printf("state: %d", state);
            case 0:
                pa_op = pa_context_get_server_info(pa_ctx, pa_server_info_cb, NULL);
                state++;
                break;
            case 1:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    // 设置默认输出设备
                    pa_op = pa_context_set_default_sink(pa_ctx, "alsa_output.pci-0000_00_1b.0.analog-stereo", NULL, NULL);
                    state++;
                }
                break;
            case 2:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    pa_context_disconnect(pa_ctx);
                    pa_context_unref(pa_ctx);
                    pa_mainloop_free(pa_ml);
                    return 0;
                }
                break;
            default:
                fprintf(stderr, "in state %d\n", state);
                return -1;
        }
        pa_mainloop_iterate(pa_ml, 1, NULL);
    }
    return 0;
}

void pa_state_cb(pa_context *c, void *userdate)
{
    pa_context_state_t state;
    int *pa_ready = userdate;

    state = pa_context_get_state(c);
    switch (state) {
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
        default:
            break;
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            *pa_ready = 2;
            break;
        case PA_CONTEXT_READY:
            *pa_ready = 1;
            break;
    }
}

void pa_server_info_cb(pa_context *c, const pa_server_info *i, void *userdate)
{
    printf("----------server_info-----------------\n");
    printf("user_name: %s\n", i->user_name);
    printf("host_name: %s\n", i->host_name);
    printf("server_version: %s\n", i->server_version);
    printf("server_name: %s\n", i->server_name);
    printf("sample_spec format:%d rate:%d channels:%d\n",
           i->sample_spec.format, i->sample_spec.rate, i->sample_spec.channels);
    printf("default_sink_name: %s\n", i->default_sink_name);
    printf("default_source_name: %s\n", i->default_source_name);
    printf("cookie: %d\n", i->cookie);
    printf("channel_map: %d\n", i->channel_map.channels);
}
