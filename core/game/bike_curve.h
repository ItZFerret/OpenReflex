/* bike_curve.h -- the parameter curves and tables of the vehicle tuning (bike_curve.c). */
#pragma once
#include "bike_math.h"
#ifdef __cplusplus
extern "C" {
#endif
float bike_lerp_clamped(float t, float a, float b);                    /* 00430810 Lerp_Clamped */
int   bike_param_curve_segment(rx_addr curve, float x, float* t);     /* (00438ee0's knot search) */
float bike_param_curve(rx_addr curve, float x);                       /* 00438ee0 ParamCurve_EvalCubic */
float bike_indexed_curve(rx_addr table, float x);                     /* 004390d0 IndexedCurve_Interp */
#ifdef __cplusplus
}
#endif
