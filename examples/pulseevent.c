/*                                                                              
 * Copyright (C) 2013 Deepin, Inc.                                              
 *               2013 Zhai Xiang                                                
 *                                                                              
 * Author:     Zhai Xiang <zhaixiang@linuxdeepin.com>                           
 * Maintainer: Zhai Xiang <zhaixiang@linuxdeepin.com>                           
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
#include <pthread.h>

static pthread_mutex_t m_mutex;
static int m_state = 0;
static pa_context *m_pa_ctx = NULL;

static void m_pa_state_cb(pa_context *c, void *userdata);
static void *m_pa_mainloop_cb(void *arg);
static void m_pa_context_subscribe_cb(pa_context *c, 
                                      pa_subscription_event_type_t t, 
                                      uint32_t idx, 
                                      void *userdata);
static void m_pa_client_info_cb(pa_context *c,                                  
                                const pa_client_info *i,                        
                                int eol,                                        
                                void *userdata);

int main(int argc, char *argv[]) {
    pthread_t thread;

    pthread_mutex_init(&m_mutex, NULL);
    pthread_create(&thread, NULL, m_pa_mainloop_cb, NULL);

    sleep(3);
    if (m_pa_ctx) {
        pa_context_set_sink_mute_by_index(m_pa_ctx, 1, 1, NULL, NULL);
    }

    while (1) {
        sleep(1);
    }

    pthread_mutex_destroy(&m_mutex);

    return 0;
}

/* http://freedesktop.org/software/pulseaudio/doxygen/subscribe.html */
static void *m_pa_mainloop_cb(void *arg) {
    // Define our pulse audio loop and connection variables
    pa_mainloop *pa_ml = NULL;
    pa_mainloop_api *pa_mlapi = NULL;
    pa_operation *pa_op = NULL;
    int pa_ready;

RE_CONN:
    // We'll need these state variables to keep track of our requests
    m_state = 0;
    pa_ready = 0;

    // Create a mainloop API and connection to the default server
    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    m_pa_ctx = pa_context_new(pa_mlapi, "test");

    // This function connects to the pulse server
    pa_context_connect(m_pa_ctx, NULL, 0, NULL);

    // This function defines a callback so the server will tell us it's state.
    // Our callback will wait for the state to be ready.  The callback will
    // modify the variable to 1 so we know when we have a connection and it's
    // ready.
    // If there's an error, the callback will set pa_ready to 2
    pa_context_set_state_callback(m_pa_ctx, m_pa_state_cb, &pa_ready);

    // Now we'll enter into an infinite loop until we get the data we receive
    // or if there's an error
    for (;;) {
        // We can't do anything until PA is ready, so just iterate the mainloop
        // and continue
        if (pa_ready == 0) {
            pa_mainloop_iterate(pa_ml, 1, NULL);
            continue;
        }
        // We couldn't get a connection to the server, so exit out
        if (pa_ready == 2) {
            printf("fail to connect to pulse server\n");
            printf("try to reconnect to pulse server\n");
            pa_context_disconnect(m_pa_ctx);                                      
            pa_context_unref(m_pa_ctx);                                           
            pa_mainloop_free(pa_ml);
            /* wait for a while to reconnect to pulse server */
            sleep(3);
            goto RE_CONN;
        }
        // At this point, we're connected to the server and ready to make
        // requests
        switch (m_state) {
            case 0:
                printf("try to set subscribe callback\n");
                pa_context_set_subscribe_callback(m_pa_ctx,                     
                    m_pa_context_subscribe_cb,                                  
                    NULL);
                pa_op = pa_context_subscribe(m_pa_ctx,                          
                        PA_SUBSCRIPTION_MASK_SINK|          
                        PA_SUBSCRIPTION_MASK_SOURCE|        
                        PA_SUBSCRIPTION_MASK_SINK_INPUT|       
                        PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT| 
                        PA_SUBSCRIPTION_MASK_CLIENT|        
                        PA_SUBSCRIPTION_MASK_SERVER|        
                        PA_SUBSCRIPTION_MASK_CARD,                               
                        NULL,                                                   
                        NULL);
                m_state++;
                break;
            case 1:
                usleep(100);
                break;
            case 2:
                // Now we're done, clean up and disconnect and return
                printf("disconnect pulse server\n");
                pa_context_disconnect(m_pa_ctx);
                pa_context_unref(m_pa_ctx);
                pa_mainloop_free(pa_ml);
                return NULL;
            default:
                // We should never see this state
                fprintf(stderr, "in state %d\n", m_state);
                return NULL;
        }
        // Iterate the main loop and go again.  The second argument is whether
        // or not the iteration should block until something is ready to be
        // done.  Set it to zero for non-blocking.
        pa_mainloop_iterate(pa_ml, 1, NULL);
    }

    return NULL;
}

// This callback gets called when our context changes state.  We really only
// care about when it's ready or if it has failed
static void m_pa_state_cb(pa_context *c, void *userdata) {
    pthread_mutex_lock(&m_mutex);
    pa_context_state_t state;
    int *pa_ready = userdata;
    
    state = pa_context_get_state(c);
    switch (state) {
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
    pthread_mutex_unlock(&m_mutex);
}

static void m_pa_client_info_cb(pa_context *c, 
                                const pa_client_info *i, 
                                int eol, 
                                void *userdata) 
{
    printf("DEBUG client info %s\n", i ? i->name : NULL);
}

static void m_pa_sink_info_cb(pa_context *c,                                    
                              const pa_sink_info *i,                            
                              int eol,                                          
                              void *userdata)                                   
{                                                                               
    printf("DEBUG sink info %s %d\n", i ? i->name : NULL, i ? i->index : 0);                                 
}

static void m_pa_context_subscribe_cb(pa_context *c, 
                                      pa_subscription_event_type_t t, 
                                      uint32_t idx, 
                                      void *userdata) 
{
    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
        case PA_SUBSCRIPTION_EVENT_SINK:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
                printf("DEBUG sink %d removed\n", idx);
            } else {                                                              
                pa_context_get_sink_info_by_index(c, idx, m_pa_sink_info_cb, NULL);
            }
            break;
        case PA_SUBSCRIPTION_EVENT_CLIENT:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
                printf("DEBUG client %d removed\n", idx);
            } else {
                printf("DEBUG client %d inserted\n", idx);
                pa_context_get_client_info(c, idx, m_pa_client_info_cb, NULL);
            }
            break;
    }
}
