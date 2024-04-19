#ifdef VIA_ENABLE

    #include "via.h"
    #include "rgb_via.h"
    #include "rgb_matrix.h"
    #include "quantum.h"
    #include "eeprom.h"
// Lege eine globale Config für die Werte jeder einzelner Taste
rgb_per_key_settings_config g_rgb_p_key_config;

void values_load(void)
{
    eeprom_read_block( &g_rgb_p_key_config, ((void*)VIA_EEPROM_CUSTOM_CONFIG_ADDR), sizeof(g_rgb_p_key_config) );
}

void values_save(void)
{
    eeprom_update_block( &g_rgb_p_key_config, ((void*)VIA_EEPROM_CUSTOM_CONFIG_ADDR), sizeof(g_rgb_p_key_config) );
}


void via_init_kb(void)
{
    // If the EEPROM has the magic, the data is good.
    // OK to load from EEPROM
    if (via_eeprom_is_valid()) {
        values_load();
    } else	{
        values_save();
        // DO NOT set EEPROM valid here, let caller do this
    }
}


void updateKeyboardColor(void){
    if(g_rgb_p_key_config.isActive){ // if isActivate-Ttggle is active, load on startup the config
        rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_individual_rgb);
        for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) { // Number of keyboard-keys - 
            RGB rgb = hsv_to_rgb((HSV){ .h = g_rgb_p_key_config.color[i].h,
                                        .s = g_rgb_p_key_config.color[i].s,
                                        .v = rgb_matrix_get_val()  } );
                                        
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
        }
    }
}


void rgb_per_key_matrix_set_value(uint8_t *data) {
    // data = [ value_id, value_data ]
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);

    switch (*value_id) {

        //TODO Do i need a case brightness or is the default ok? 
        case id_rgb_per_key_matrix_color: {
            uint8_t index = value_data[0]; // == 0,1,2
            if ( index >= 0 && index < RGB_MATRIX_LED_COUNT ){
            	RGB rgb = hsv_to_rgb( (HSV){ .h = value_data[1],
            								 .s = value_data[2],
											 .v = rgb_matrix_get_val() });

                g_rgb_p_key_config.color[index].h = value_data[1];
				g_rgb_p_key_config.color[index].s = value_data[2];

                rgb_matrix_set_color(index, rgb.r, rgb.g, rgb.b);
            }
            break;
        }
        case id_rgb_per_key_matrix_primary_color: {
            g_rgb_p_key_config.primaryColor.h = value_data[0];
            g_rgb_p_key_config.primaryColor.s = value_data[1];
            RGB rgb = hsv_to_rgb( (HSV){ .h = value_data[0],
            							 .s = value_data[1],
										 .v = rgb_matrix_get_val() });
            rgb_matrix_set_color_all(rgb.r, rgb.g, rgb.b);       


            for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) { // Number of keyboard-keys - 
                g_rgb_p_key_config.color[i].h = g_rgb_p_key_config.primaryColor.h;
                g_rgb_p_key_config.color[i].s = g_rgb_p_key_config.primaryColor.s;
                g_rgb_p_key_config.color[i].v = rgb_matrix_get_val();  
            }                          
            break;
        }        
        case id_rgb_per_key_is_active: {
			g_rgb_p_key_config.isActive = *value_data;  
            updateKeyboardColor();
            break;
        }
    }
}

void rgb_per_key_matrix_get_value( uint8_t *data )
{
    
    // data = [ value_id, value_data ]
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);

    switch ( *value_id )
    {
        //TODO Do i need a case brightness or is the default ok? 
        case id_rgb_per_key_matrix_color:
        {
            uint8_t index = value_data[0]; // == 0,1,2
            value_data[1] = g_rgb_p_key_config.color[index].h;
            value_data[2] = g_rgb_p_key_config.color[index].s;
            break;
        }
        case id_rgb_per_key_matrix_primary_color:
        {
            value_data[0] = g_rgb_p_key_config.primaryColor.h;
            value_data[1] = g_rgb_p_key_config.primaryColor.s;
            break;
        }
        case id_rgb_per_key_is_active:
        {
            *value_data = g_rgb_p_key_config.isActive;
            break;
        }
    }
}


void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    // data = [ command_id, channel_id, value_id, value_data ]
    uint8_t *command_id        = &(data[0]);
    uint8_t *channel_id        = &(data[1]);
    uint8_t *value_id_and_data = &(data[2]);

    if ( *channel_id == id_custom_channel ) {
        switch ( *command_id )
        {
            case id_custom_set_value:
            {
            	rgb_per_key_matrix_set_value(value_id_and_data);
                break;
            }
            case id_custom_get_value:
            {
                rgb_per_key_matrix_get_value(value_id_and_data);
                break;
            }
            case id_custom_save:
            {
                values_save();
                break;
            }
            default:
            {
                // Unhandled message.
                *command_id = id_unhandled;
                break;
            }
        }
        return;
    }

    // Return the unhandled state
    *command_id = id_unhandled;

    // DO NOT call raw_hid_send(data,length) here, let caller do this
}
void keyboard_post_init_user(void) {
  // Call the post init code.
    via_init_kb();
    updateKeyboardColor();
}

#endif