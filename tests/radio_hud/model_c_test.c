#include "hit_radio.h"
#include "hit_radio_model.h"

int radio_model_c_test(void) {
    HitRadioModel model;
    hit_radio_model_reset(&model);
    return hit_radio_model_show(&model, 4, 3, 7);
}
