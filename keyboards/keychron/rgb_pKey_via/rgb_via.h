#pragma once

//#include "quantum.h"
#include "rgb_matrix.h"


#ifdef VIA_ENABLE
#    include "via.h"

// VIA Custom Settings 104* 3 bytes (HSV) + 1 Byte int = 313 bytes
#ifdef VIA_EEPROM_CUSTOM_CONFIG_SIZE
#undef VIA_EEPROM_CUSTOM_CONFIG_SIZE
#endif
#define VIA_EEPROM_CUSTOM_CONFIG_SIZE (RGB_MATRIX_LED_COUNT * 3 + 1 + 5) 

    // Handler fuer via
    enum via_qmk_rgb_per_key_value {

       id_rgb_per_key_matrix_color = 1,
       id_rgb_per_key_is_active = 2,
       id_rgb_per_key_matrix_primary_color = 3
    };


    // struct to save things
    typedef struct {
        HSV color[RGB_MATRIX_LED_COUNT]; //Anzahl der Tasten
        HSV primaryColor;
        uint8_t isActive;
    } rgb_per_key_settings_config;



#endif
