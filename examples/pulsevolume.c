/*                                                                              
 * Copyright (C) 2013 Deepin, Inc.                                              
 *               2013 Zhai Xiang                                                
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

int main(int argc, char **argv)
{
    pa_cvolume volume, *new_volume;
    pa_channel_map channel_map;
    float balance;
    char buf[1024];
    int i;

    memset(&volume, 0, sizeof(pa_cvolume));
    volume.channels = 4;
    volume.values[0] = 1500;
    volume.values[1] = 1000;
    volume.values[2] = 1000;
    volume.values[3] = 1000;
    //pa_cvolume_set(&volume, 4, 1500);

    memset(&channel_map, 0, sizeof(pa_channel_map));
    channel_map.channels = 4;
    channel_map.map[0] = 1;
    channel_map.map[1] = 2;
    channel_map.map[2] = 8;
    channel_map.map[3] = 9;

    printf("PA_CVOLUME_SNPRINT_MAX: %d\n", PA_CVOLUME_SNPRINT_MAX);
    printf("PA_DECIBEL_MININFTY:%f\n", PA_DECIBEL_MININFTY);
    printf("PA_SW_CVOLUME_SNPRINT_DB_MAX: %d\n", PA_SW_CVOLUME_SNPRINT_DB_MAX);
    printf("PA_SW_VOLUME_SNPRINT_DB_MAX: %d\n", PA_SW_VOLUME_SNPRINT_DB_MAX);
    printf("PA_VOLUME_INVALID: %u\n", PA_VOLUME_INVALID);
    printf("PA_VOLUME_MAX: %d\n", PA_VOLUME_MAX);
    printf("PA_VOLUME_MUTED: %d\n", PA_VOLUME_MUTED);
    printf("PA_VOLUME_NORM: %d\n", PA_VOLUME_NORM);
    printf("PA_VOLUME_SNPRINT_MAX: %d\n", PA_VOLUME_SNPRINT_MAX);
    printf("PA_VOLUME_UI_MAX: %d\n", PA_VOLUME_UI_MAX);

    printf("mute dB: %f\n", pa_sw_volume_to_dB(PA_VOLUME_MUTED));
    printf("normal dB: %f\n", pa_sw_volume_to_dB(PA_VOLUME_NORM));
    printf("ui_max dB: %f\n", pa_sw_volume_to_dB(PA_VOLUME_UI_MAX));

    printf("get_balance:%f\n", pa_cvolume_get_balance(&volume, &channel_map));
    printf("volume = %s\n", pa_cvolume_snprint(buf, sizeof(buf)-1, &volume));
    for (i = 0; i < volume.channels; i++) {
        printf("\t%d channel volume: %d\n", i, volume.values[i]);
    }
    printf("map = %s\n", pa_channel_map_snprint(buf, sizeof(buf)-1, &channel_map));
    printf("input new balance:");
    scanf("%f", &balance);

    new_volume = pa_cvolume_set_balance(&volume, &channel_map, balance);
    printf("after get_balance:%f\n", pa_cvolume_get_balance(&volume, &channel_map));
    printf("after volume = %s\n", pa_cvolume_snprint(buf, sizeof(buf)-1, &volume));
    for (i = 0; i < volume.channels; i++) {
        printf("\t%d channel volume: %d\n", i, volume.values[i]);
    }
    printf("after map = %s\n", pa_channel_map_snprint(buf, sizeof(buf)-1, &channel_map));
    return 0;
}
