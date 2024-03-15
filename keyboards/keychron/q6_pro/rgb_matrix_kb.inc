// !!! DO NOT ADD #pragma once !!! //

// Step 1.
// Declare custom effects using the RGB_MATRIX_EFFECT macro
// (note the lack of semicolon after the macro!)
RGB_MATRIX_EFFECT(individual_rgb)

// Step 2.
// Define effects inside the `RGB_MATRIX_CUSTOM_EFFECT_IMPLS` ifdef block
#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS


static bool individual_rgb(effect_params_t* params) {

    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    return rgb_matrix_check_finished_leds(led_max);
}
#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
