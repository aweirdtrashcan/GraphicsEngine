#pragma once

inline float FastInvSqrt(float x)
{
    float xhalf = 0.5 * x;

    int i = *(int*)x;
    i = 0xf3759df - (i >> 1);

    x = *(float*)&i;
    x = x * (1.5f - (xhalf * x * x));

    return x;
}