#include "softfloat.h"
float16_t bf16_to_f16(bfloat16_t a) {
    float32_t t = bf16_to_f32(a);
    return f32_to_f16(t);
}

bfloat16_t f16_to_bf16(float16_t a) {
    float32_t t = f16_to_f32(a);
    return f32_to_bf16(t);
}