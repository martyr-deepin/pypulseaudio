#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>

static pa_threaded_mainloop *m_pa_ml = NULL;
static pa_context *m_pa_ctx = NULL;
static pa_mainloop_api *m_pa_mlapi = NULL;
static int m_channel_num = 0;
static GtkAdjustment *m_adjust = NULL;

static void m_pa_sink_event_cb(pa_context *c,                                   
                               const pa_sink_info *info,                        
                               int eol,                                         
                               void *user_data);
static void m_pa_context_subscribe_cb(pa_context *c,                            
                                      pa_subscription_event_type_t t,           
                                      uint32_t idx,                             
                                      void *user_data);
static void m_pa_sink_info_cb(pa_context *c,                                    
                              const pa_sink_info *i,                            
                              int eol,                                          
                              void *userdata);
static void m_context_state_cb(pa_context *c, void *user_data);
static void m_connect_to_pulse();
static void m_set_sink_volume(int idx, int volume);
static void m_value_changed(GtkAdjustment *adjust, gpointer user_data);

static void m_pa_sink_event_cb(pa_context *c,                                     
                               const pa_sink_info *info,                          
                               int eol,                                           
                               void *user_data)                                    
{                                                                               
    if (!c || !info || eol > 0 || !user_data) {
        return;
    }
    
    printf("DEBUG %s\n", user_data);
    m_pa_sink_info_cb(c, info, eol, NULL);
}

static void m_pa_context_subscribe_cb(pa_context *c,                            
                                      pa_subscription_event_type_t t,           
                                      uint32_t idx,                             
                                      void *user_data)                           
{                                                                               
    if (!c) {
        printf("m_pa_context_subscribe_cb() invalid arguement\n");
        return;
    }
                                                                                
    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {                          
        case PA_SUBSCRIPTION_EVENT_SINK:                                        
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_sink_info_by_index(c, idx, m_pa_sink_event_cb, "sink_new");
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_sink_info_by_index(c, idx, m_pa_sink_event_cb, "sink_changed");
            }                                                                   
            break;
        /* TODO: it does not need to test so much kind of event signal
        case PA_SUBSCRIPTION_EVENT_SOURCE:                                      
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_source_info_by_index(c, idx, m_pa_source_new_cb, NULL);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_source_info_by_index(c, idx, m_pa_source_changed_cb, NULL);
            }                                                                   
            break;
        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:                                  
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_sink_input_info(c, idx, m_pa_sink_input_new_cb, NULL);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_sink_input_info(c, idx, m_pa_sink_input_changed_cb, NULL);
            }                                                                   
            break;                                                              
        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:                               
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_source_output_info(c, idx, m_pa_source_output_new_cb, NULL);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_source_output_info(c, idx, m_pa_source_output_changed_cb, NULL);
            }                                                                   
            break;                                                              
        case PA_SUBSCRIPTION_EVENT_CLIENT:                                      
            break;                                                              
        case PA_SUBSCRIPTION_EVENT_SERVER:                                      
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_server_info(c, m_pa_server_new_cb, NULL);        
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_server_info(c, m_pa_server_changed_cb, NULL);    
            }                                                                   
            break;
        case PA_SUBSCRIPTION_EVENT_CARD:                                        
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_context_get_card_info_by_index(c, idx, m_pa_card_new_cb, NULL);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
                pa_context_get_card_info_by_index(c, idx, m_pa_card_changed_cb, NULL);
            }                                                                   
            break;
        */
    }                                                                           
}

static void m_pa_sink_info_cb(pa_context *c, 
                              const pa_sink_info *i, 
                              int eol, 
                              void *userdata)
{
    if (!c || !i || eol > 0) {
        return;
    }

    pa_sink_port_info **ports  = NULL;
    pa_sink_port_info *port = NULL;
    pa_sink_port_info *active_port = NULL;
    const char *prop_key = NULL;
    void *prop_state = NULL;
    int j;

    while ((prop_key = pa_proplist_iterate(i->proplist, &prop_state))) {
        printf("DEBUG %s %s\n", prop_key, pa_proplist_gets(i->proplist, prop_key));
    }

    m_channel_num = i->channel_map.channels;
    printf("DEBUG channel_map_can_balance %s, channel_map_count %d\n", 
           pa_channel_map_can_balance(&i->channel_map) ? "TRUE" : "FALSE", 
           i->channel_map.channels);
    for (j = 0; j < i->channel_map.channels; j++) {
        printf("DEBUG channel_map %d\n", i->channel_map.map[j]);
    }

    ports = i->ports;
    for (j = 0; j < i->n_ports; j++) {
        port = ports[j];
        printf("DEBUG port %s %s %s\n", 
               port->name, 
               port->description, 
               port->available ? "TRUE" : "FALSE");
    }

    active_port = i->active_port;
    if (active_port) {
        printf("DEBUG active_port %s %s %s\n", 
               active_port->name, 
               active_port->description, 
               active_port->available ? "TRUE" : "FALSE");
    }

    for (j = 0; j < i->volume.channels; j++) {
        printf("DEBUG volume_channel_value %d\n", i->volume.values[j]);
    }
    gtk_adjustment_set_value(m_adjust, i->volume.values[0]);

    printf("DEBUG sink %s %s base_volume %d muted %s\n", 
           i->name, 
           i->description, 
           i->base_volume, 
           i->mute ? "TRUE" : "FALSE");
}

static void m_context_state_cb(pa_context *c, void *user_data)                   
{                                                                               
    if (!c) {
        printf("m_context_state_cb() invalid arguement\n");
        return;
    }

    pa_operation *pa_op = NULL;

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_UNCONNECTED:                                            
        case PA_CONTEXT_CONNECTING:                                             
        case PA_CONTEXT_AUTHORIZING:                                            
        case PA_CONTEXT_SETTING_NAME:                                           
            break;                                                              
                                                                                
        case PA_CONTEXT_READY:                                                  
            pa_context_set_subscribe_callback(c, m_pa_context_subscribe_cb, NULL);
                                                                                
            pa_op = pa_context_subscribe(c, (pa_subscription_mask_t)      
                                           (PA_SUBSCRIPTION_MASK_SINK|          
                                            PA_SUBSCRIPTION_MASK_SOURCE|        
                                            PA_SUBSCRIPTION_MASK_SINK_INPUT|    
                                            PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT| 
                                            PA_SUBSCRIPTION_MASK_CLIENT|        
                                            PA_SUBSCRIPTION_MASK_SERVER|        
                                            PA_SUBSCRIPTION_MASK_CARD), NULL, NULL);
            if (!pa_op) {
                printf("pa_context_subscribe() failed\n");
                return;
            }
            pa_operation_unref(pa_op);

            pa_op = pa_context_get_sink_info_list(c, m_pa_sink_info_cb, NULL);
            if (!pa_op) {
                printf("pa_context_get_sink_info_list() failed\n");
                return;
            }
            pa_operation_unref(pa_op);
            break;                                                              
                                                                                
        case PA_CONTEXT_FAILED:                                                 
            if (m_pa_ctx) {
                pa_context_unref(m_pa_ctx);                                     
                m_pa_ctx = NULL;
            }            
                                                                                
            printf("Connection failed, attempting reconnect\n");                
            g_timeout_add_seconds(13, m_connect_to_pulse, NULL);                                           
            return;                                                             
                                                                                
        case PA_CONTEXT_TERMINATED:                                             
        default:                                                                
            printf("pa_context terminated\n");                                  
            return;                                                             
    }                                                                           
}

static void m_connect_to_pulse() 
{
    m_pa_ctx = pa_context_new(m_pa_mlapi, "PulseAudio Gtk Demo");
    if (!m_pa_ctx) {
        printf("pa_context_new() failed\n");
        return;
    }
    
    pa_context_set_state_callback(m_pa_ctx, m_context_state_cb, NULL);
    
    if (pa_context_connect(m_pa_ctx, NULL, PA_CONTEXT_NOFAIL, NULL) < 0) {
        if (pa_context_errno(m_pa_ctx) == PA_ERR_INVALID) {
            printf("Connection to PulseAudio failed. Automatic retry in 13s\n");
            return;
        }
    }
}

static void m_set_sink_volume(int idx, int volume) 
{
    pa_operation *pa_op = NULL;
    pa_cvolume sink_volume;
    int i;

    printf("DEBUG m_set_sink_volume channel_num %d\n", m_channel_num);
    sink_volume.channels = m_channel_num;
    for (i = 0; i < m_channel_num; i++) {
        sink_volume.values[i] = volume;
    }

    pa_op = pa_context_set_sink_volume_by_index(m_pa_ctx, idx, &sink_volume, NULL, NULL);
    if (!pa_op) {
        printf("pa_context_set_sink_volume_by_index() failed");
        return;
    }
    pa_operation_unref(pa_op);
    pa_op = NULL;
}

static void m_value_changed(GtkAdjustment *adjust, gpointer user_data) 
{
    double value = gtk_adjustment_get_value(adjust);

    printf("DEBUG value-changed %f\n", value);
    // you can change the index (here is 1) based on your output devices
    m_set_sink_volume(1, (int)value);
}

static void m_destroy(GtkWindow *window, gpointer user_data) 
{
    if (m_pa_ctx) {
        pa_context_unref(m_pa_ctx);
        m_pa_ctx = NULL;
    }

    if (m_pa_ml) {
        pa_threaded_mainloop_stop(m_pa_ml);
        pa_threaded_mainloop_free(m_pa_ml);
        m_pa_ml = NULL;
    }

    gtk_main_quit();
}

int main(int argc, char **argv)
{
    GtkWidget *window = NULL;
    GtkWidget *scale = NULL;
    
    m_pa_ml = pa_threaded_mainloop_new();
    if (!m_pa_ml) {
        printf("pa_threaded_mainloop_new() failed");
        return -1;
    }
    
    m_pa_mlapi = pa_threaded_mainloop_get_api(m_pa_ml);
    if (!m_pa_mlapi) {
        printf("pa_threaded_mainloop_get_api() failed");
        return -1;
    }

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "PulseAudio Gtk Demo");
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(m_destroy), NULL);

    m_adjust = gtk_adjustment_new(60000, 0, 100000, 1, 1, 0);
    g_signal_connect(m_adjust, "value-changed", G_CALLBACK(m_value_changed), NULL);
    
    scale = gtk_hscale_new(m_adjust);
    gtk_container_add(GTK_CONTAINER(window), scale);

    gtk_widget_show(scale);
    gtk_widget_show(window);
  
    pa_threaded_mainloop_start(m_pa_ml);
    m_connect_to_pulse();
    
    gtk_main();

    return 0;
}
