/* Copyright 2023 @ Keychron (https://www.keychron.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include "q6_pro.h"
#include "rgb_via.h"
#include "rgb_matrix.h"


#ifdef KC_BLUETOOTH_ENABLE
#    include "ckbt51.h"
#    include "bluetooth.h"
#    include "indicator.h"
#    include "transport.h"
#    include "battery.h"
#    include "bat_level_animation.h"
#    include "lpm.h"
#endif

#ifdef ENABLE_FACTORY_TEST
#    include "factory_test.h"
#endif

#define POWER_ON_LED_DURATION 3000

typedef struct PACKED {
    uint8_t len;
    uint8_t keycode[3];
} key_combination_t;

static uint32_t factory_timer_buffer            = 0;
static uint32_t power_on_indicator_timer_buffer = 0;
static uint32_t siri_timer_buffer               = 0;
static uint8_t  mac_keycode[4]                  = {KC_LOPT, KC_ROPT, KC_LCMD, KC_RCMD};

key_combination_t key_comb_list[4] = {
    {2, {KC_LWIN, KC_TAB}},        // Task (win)
    {2, {KC_LWIN, KC_E}},          // Files (win)
    {3, {KC_LSFT, KC_LGUI, KC_4}}, // Snapshot (mac)
    {2, {KC_LWIN, KC_C}}           // Cortana (win)
};

#ifdef KC_BLUETOOTH_ENABLE
bool                   firstDisconnect  = true;
bool                   bt_factory_reset = false;
static virtual_timer_t pairing_key_timer;
extern uint8_t         g_pwm_buffer[DRIVER_COUNT][192];

static void pairing_key_timer_cb(void *arg) {
    bluetooth_pairing_ex(*(uint8_t *)arg, NULL);
}
#endif

bool dip_switch_update_kb(uint8_t index, bool active) {
    if (index == 0) {
#ifdef INVERT_OS_SWITCH_STATE
        default_layer_set(1UL << (!active ? 2 : 0));
#else
        default_layer_set(1UL << (active ? 2 : 0));
#endif
    }
    dip_switch_update_user(index, active);

    return true;
}

#ifdef KC_BLUETOOTH_ENABLE
bool process_record_kb_bt(uint16_t keycode, keyrecord_t *record) {
#else
bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
#endif
    static uint8_t host_idx = 0;

    switch (keycode) {
        case KC_LOPTN:
        case KC_ROPTN:
        case KC_LCMMD:
        case KC_RCMMD:
            if (record->event.pressed) {
                register_code(mac_keycode[keycode - KC_LOPTN]);
            } else {
                unregister_code(mac_keycode[keycode - KC_LOPTN]);
            }
            return false; // Skip all further processing of this key)
        case KC_TASK:
            if (record->event.pressed) {
                ckbt51_factory_reset();
            }
        case KC_FILE:
        case KC_SNAP:
        case KC_CTANA:
            if (record->event.pressed) {
                for (uint8_t i = 0; i < key_comb_list[keycode - KC_TASK].len; i++)
                    register_code(key_comb_list[keycode - KC_TASK].keycode[i]);
            } else {
                for (uint8_t i = 0; i < key_comb_list[keycode - KC_TASK].len; i++)
                    unregister_code(key_comb_list[keycode - KC_TASK].keycode[i]);
            }
            return false; // Skip all further processing of this key
        case KC_SIRI:
            if (record->event.pressed && siri_timer_buffer == 0) {
                register_code(KC_LGUI);
                register_code(KC_SPACE);
                siri_timer_buffer = sync_timer_read32() == 0 ? 1 : sync_timer_read32();
            }
            return false; // Skip all further processing of this key
#ifdef KC_BLUETOOTH_ENABLE
        case BT_HST1 ... BT_HST3:
            if (get_transport() == TRANSPORT_BLUETOOTH) {
                if (record->event.pressed) {
                    host_idx = keycode - BT_HST1 + 1;
                    chVTSet(&pairing_key_timer, TIME_MS2I(2000), (vtfunc_t)pairing_key_timer_cb, &host_idx);
                    bluetooth_connect_ex(host_idx, 0);
                } else {
                    host_idx = 0;
                    chVTReset(&pairing_key_timer);
                }
            }
            break;
        case BAT_LVL:
            if (get_transport() == TRANSPORT_BLUETOOTH && !usb_power_connected()) {
                bat_level_animiation_start(battery_get_percentage());
            }
            break;
#endif
        default:
#ifdef FACTORY_RESET_CHECK
            FACTORY_RESET_CHECK(keycode, record);
#endif
            break;
    }
    return true;
}

#if defined(KC_BLUETOOTH_ENABLE) && defined(ENCODER_ENABLE)
static void encoder_pad_cb(void *param) {
    encoder_inerrupt_read((uint32_t)param & 0XFF);
}
#endif

void keyboard_post_init_kb(void) {
    dip_switch_read(true);

#ifdef KC_BLUETOOTH_ENABLE
    /* Currently we don't use this reset pin */
    // palSetLineMode(CKBT51_RESET_PIN, PAL_MODE_UNCONNECTED);
    palSetLineMode(CKBT51_RESET_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    palWriteLine(CKBT51_RESET_PIN, PAL_HIGH);

    /* IMPORTANT: DO NOT enable internal pull-up resistor
     * as there is an external pull-down resistor.
     */
    palSetLineMode(USB_BT_MODE_SELECT_PIN, PAL_MODE_INPUT);

    ckbt51_init(false);
    bluetooth_init();

    power_on_indicator_timer_buffer = sync_timer_read32() | 1;
    writePin(BAT_LOW_LED_PIN, BAT_LOW_LED_PIN_ON_STATE);

#    ifdef ENCODER_ENABLE
    pin_t encoders_pad_a[NUM_ENCODERS] = ENCODERS_PAD_A;
    pin_t encoders_pad_b[NUM_ENCODERS] = ENCODERS_PAD_B;
    for (uint32_t i = 0; i < NUM_ENCODERS; i++) {
        palEnableLineEvent(encoders_pad_a[i], PAL_EVENT_MODE_BOTH_EDGES);
        palEnableLineEvent(encoders_pad_b[i], PAL_EVENT_MODE_BOTH_EDGES);
        palSetLineCallback(encoders_pad_a[i], encoder_pad_cb, (void *)i);
        palSetLineCallback(encoders_pad_b[i], encoder_pad_cb, (void *)i);
    }
#    endif
#endif

    keyboard_post_init_user();
}

static void ckbt51_param_init(void);

void matrix_scan_kb(void) {
    if (factory_timer_buffer && timer_elapsed32(factory_timer_buffer) > 2000) {
        factory_timer_buffer = 0;
        if (bt_factory_reset) {
            bt_factory_reset = false;
            palWriteLine(CKBT51_RESET_PIN, PAL_LOW);
            wait_ms(5);
            palWriteLine(CKBT51_RESET_PIN, PAL_HIGH);
        }
    }

    if (power_on_indicator_timer_buffer) {
        if (sync_timer_elapsed32(power_on_indicator_timer_buffer) > POWER_ON_LED_DURATION) {
            power_on_indicator_timer_buffer = 0;
            writePin(BAT_LOW_LED_PIN, !BAT_LOW_LED_PIN_ON_STATE);
        } else {
            writePin(BAT_LOW_LED_PIN, BAT_LOW_LED_PIN_ON_STATE);
        }
    }

    if (siri_timer_buffer && sync_timer_elapsed32(siri_timer_buffer) > 500) {
        siri_timer_buffer = 0;
        unregister_code(KC_LGUI);
        unregister_code(KC_SPACE);
    }

#ifdef FACTORY_RESET_TASK
    FACTORY_RESET_TASK();
#endif
    matrix_scan_user();
}

#ifdef KC_BLUETOOTH_ENABLE
static void ckbt51_param_init(void) {
    /* Set bluetooth device name */
    ckbt51_set_local_name(PRODUCT);
    wait_ms(10);
    /* Set bluetooth parameters */
    module_param_t param = {.event_mode             = 0x02,
                            .connected_idle_timeout = 7200,
                            .pairing_timeout        = 180,
                            .pairing_mode           = 0,
                            .reconnect_timeout      = 5,
                            .report_rate            = 90,
                            .vendor_id_source       = 1,
                            .verndor_id             = 0, // Must be 0x3434
                            .product_id             = PRODUCT_ID};
    ckbt51_set_param(&param);
    wait_ms(10);
}

void bluetooth_enter_disconnected_kb(uint8_t host_idx) {
    if (bt_factory_reset) {
        ckbt51_param_init();
        factory_timer_buffer = timer_read32();
    }
    /* CKBT51 bluetooth module boot time is slower, it enters disconnected after boot,
       so we place initialization here. */
    if (firstDisconnect && sync_timer_read32() < 1000 && get_transport() == TRANSPORT_BLUETOOTH) {
        ckbt51_param_init();
        bluetooth_connect();
        firstDisconnect = false;
    }
}

void ckbt51_default_ack_handler(uint8_t *data, uint8_t len) {
    if (data[1] == 0x45) {
        module_param_t param = {.event_mode             = 0x02,
                                .connected_idle_timeout = 7200,
                                .pairing_timeout        = 180,
                                .pairing_mode           = 0,
                                .reconnect_timeout      = 5,
                                .report_rate            = 90,
                                .vendor_id_source       = 1,
                                .verndor_id             = 0, // Must be 0x3434
                                .product_id             = PRODUCT_ID};
        ckbt51_set_param(&param);
    }
}

void bluetooth_pre_task(void) {
    static uint8_t mode = 1;

    if (readPin(USB_BT_MODE_SELECT_PIN) != mode) {
        if (readPin(USB_BT_MODE_SELECT_PIN) != mode) {
            mode = readPin(USB_BT_MODE_SELECT_PIN);
            set_transport(mode == 0 ? TRANSPORT_BLUETOOTH : TRANSPORT_USB);
        }
    }
}
#endif

void battery_calculte_voltage(uint16_t value) {
    uint16_t voltage = ((uint32_t)value) * 2246 / 1000;

#ifdef LED_MATRIX_ENABLE
    if (led_matrix_is_enabled()) {
        uint32_t totalBuf = 0;

        for (uint8_t i = 0; i < DRIVER_COUNT; i++)
            for (uint8_t j = 0; j < 192; j++)
                totalBuf += g_pwm_buffer[i][j];
        /* We assumpt it is linear relationship*/
        voltage += (30 * totalBuf / LED_MATRIX_LED_COUNT / 255);
    }
#endif
#ifdef RGB_MATRIX_ENABLE
    if (rgb_matrix_is_enabled()) {
        uint32_t totalBuf = 0;

        for (uint8_t i = 0; i < DRIVER_COUNT; i++)
            for (uint8_t j = 0; j < 192; j++)
                totalBuf += g_pwm_buffer[i][j];
        /* We assumpt it is linear relationship*/
        uint32_t compensation = 60 * totalBuf / RGB_MATRIX_LED_COUNT / 255 / 3;
        voltage += compensation;
    }
#endif
    battery_set_voltage(voltage);
}

bool via_command_kb(uint8_t *data, uint8_t length) {
    switch (data[0]) {
#ifdef KC_BLUETOOTH_ENABLE
        case 0xAA:
            ckbt51_dfu_rx(data, length);
            break;
#endif
#ifdef ENABLE_FACTORY_TEST
        case 0xAB:
            factory_test_rx(data, length);
            break;
#endif
        default:
            return false;
    }

    return true;
}

#if !defined(VIA_ENABLE)
void raw_hid_receive(uint8_t *data, uint8_t length) {
    switch (data[0]) {
        case RAW_HID_CMD:
            via_command_kb(data, length);
            break;
    }
}
#endif










#ifdef VIA_ENABLE

    #include "via.h"
    #include "rgb_via.h"
    #include "quantum.h"
    #include "eeprom.h"

#define TRUE 1
#define FALSE 0

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




void rgb_per_key_matrix_set_value(uint8_t *data) {
    // data = [ value_id, value_data ]
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);

    switch (*value_id) {

        case id_rgb_per_key_matrix_color: {
            uint8_t index = value_data[0]; // == 0,1,2
            if ( index >= 0 && index < 104 ){
            	RGB rgb = hsv_to_rgb( (HSV){ .h = value_data[1],
            								 .s = value_data[2],
											 .v = rgb_matrix_get_val() });

                g_rgb_p_key_config.color[index].h = value_data[1];
				g_rgb_p_key_config.color[index].s = value_data[2];

                rgb_matrix_set_color(index, rgb.r, rgb.g, rgb.b);
            }
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
        case id_rgb_per_key_matrix_color:
        {
            uint8_t index = value_data[0]; // == 0,1,2
            value_data[1] = g_rgb_p_key_config.color[index].h;
            value_data[2] = g_rgb_p_key_config.color[index].s;
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



#include "rgb_matrix.h"

void keyboard_post_init_user(void) {
  // Call the post init code.
    via_init_kb();
    if(g_rgb_p_key_config.isActive){ // use a button in via to change the attribute
        rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_individual_rgb);
    }
 


    RGB rgb = hsv_to_rgb(rgb_matrix_config.hsv);
    for (uint8_t i = 0; i < 104; i++) {
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }


    for (uint8_t i = 0; i < 104; i++) { // Anzahl der Tasten

        RGB rgb = hsv_to_rgb((HSV){ .h = g_rgb_p_key_config.color[i].h,
                                    .s = g_rgb_p_key_config.color[i].s,
                                    .v = rgb_matrix_get_val()  } );

        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
}

#endif
