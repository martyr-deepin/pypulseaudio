/*                                                                              
 * Copyright (C) 2013 Deepin, Inc.                                              
 *               2013 Long Changjin
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
#include <string.h>
#include <pulse/pulseaudio.h>

typedef struct pa_cardlist {
    uint8_t initialized;
    uint32_t index;
    char name[512];
    //pa_card_profile_info *active_profile;
    char profile_name[512];
    char profile_description[512];
    uint32_t profile_n_sinks;
    uint32_t profile_n_sources;
    pa_proplist *proplist;
} pa_cardlist;

void pa_state_cb(pa_context *c, void *userdata);
void pa_cardlist_cb(pa_context *c, const pa_card_info *i, int eol, void *userdata);

int main(int argc, char **argv)
{
    pa_mainloop *pa_ml = NULL;
    pa_mainloop_api *pa_mlapi = NULL;
    pa_operation *pa_op = NULL;
    pa_context *pa_ctx = NULL;

    pa_cardlist cardlist[16];
    memset(cardlist, 0, sizeof(pa_cardlist)*16);

    int pa_ready = 0;
    int state = 0;
    int i = 0;


    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, "deepin");
    
    pa_context_connect(pa_ctx, NULL, 0, NULL);
    pa_context_set_state_callback(pa_ctx, pa_state_cb, &pa_ready);

    for (;;) {
        // We can't do anything until PA is ready, so just iterate the mainloop
        // and continue
        if (pa_ready == 0) {
            pa_mainloop_iterate(pa_ml, 1, NULL);
            continue;
        }
        // We couldn't get a connection to the server, so exit out
        if (pa_ready == 2) {
            pa_context_disconnect(pa_ctx);
            pa_context_unref(pa_ctx);
            pa_mainloop_free(pa_ml);
            return -1;
        }
        switch (state) {
            // State 0: we haven't done anything yet
            case 0:
                // This sends an operation to the server.  pa_sinklist_info is
                // our callback function and a pointer to our devicelist will
                // be passed to the callback The operation ID is stored in the
                // pa_op variable
                // pa_operation* pa_context_get_card_info_list(pa_context *c, pa_card_info_cb_t cb, void *userdata);
                // typedef void (*pa_card_info_cb_t) (pa_context *c, const pa_card_info*i, int eol, void *userdata);
                pa_op = pa_context_get_card_info_list(pa_ctx, pa_cardlist_cb, &cardlist);

                // Update state for next iteration through the loop
                state++;
                break;
            case 1:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    // Now we're done, clean up and disconnect and return
                    pa_operation_unref(pa_op);
                    pa_context_disconnect(pa_ctx);
                    pa_context_unref(pa_ctx);
                    pa_mainloop_free(pa_ml);

                    for (i = 0; i < 16; i++) {
                        if (! cardlist[i].initialized) break;
                        printf("card---------");
                        printf("\tindex:%d\n", cardlist[i].index);
                        printf("\tname:%s\n", cardlist[i].name);
                        printf("\tprofile name:%s\n", cardlist[i].profile_name);
                        printf("\tprofile description:%s\n", cardlist[i].profile_description);
                        printf("\tprofile n_sinks:%d\n", cardlist[i].profile_n_sinks);
                        printf("\tprofile n_sources:%d\n", cardlist[i].profile_n_sources);
                        if (pa_proplist_contains(cardlist[i].proplist, PA_PROP_DEVICE_DESCRIPTION)) {
                            printf("\tprop device description:%s\n",
                                    pa_proplist_gets(cardlist[i].proplist, PA_PROP_DEVICE_DESCRIPTION ));
                        }
                    }
                    return 0;
                }
                break;
            default:
                // We should never see this state
                fprintf(stderr, "in state %d\n", state);
                return -1;
        }
        // Iterate the main loop and go again.  The second argument is whether
        // or not the iteration should block until something is ready to be
        // done.  Set it to zero for non-blocking.
        pa_mainloop_iterate(pa_ml, 1, NULL);
    }
    return 0;
}

void pa_state_cb(pa_context *c, void *userdata)
{
        pa_context_state_t state;
        int *pa_ready = userdata;

        state = pa_context_get_state(c);
        switch  (state) {
                // There are just here for reference
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

void pa_cardlist_cb(pa_context *c, const pa_card_info *i, int eol, void *userdata)
{
    pa_cardlist *cardlist = userdata;
    int n = 0;
    // If eol is set to a positive number, you're at the end of the list
    printf("in cardlist_cb eol:%d\n", eol);
    if (eol > 0) {
        return;
    }
    for (n = 0; n < 16; n++) {
        if (cardlist[n].initialized) {
            continue;
        }
        cardlist[n].initialized = 1;
        cardlist[n].index = i->index;
        strncpy(cardlist[n].name, i->name, 511);
        strncpy(cardlist[n].profile_name, i->active_profile->name, 511);
        strncpy(cardlist[n].profile_description, i->active_profile->description, 511);
        cardlist[n].profile_n_sinks = i->active_profile->n_sinks;
        cardlist[n].profile_n_sources = i->active_profile->n_sources;
        cardlist[n].proplist = pa_proplist_copy(i->proplist);
        break;
    }
}
