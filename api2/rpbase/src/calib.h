/**
 * $Id: $
 *
 * @brief Red Pitaya Calibration Module.
 *
 * @Author Red Pitaya
 *
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#ifndef __CALIB_H
#define __CALIB_H

#include <stdint.h>

#include "redpitaya/rp.h"

#define CONSTANT_SIGNAL_AMPLITUDE 0.8

int calib_Init();
int calib_Release();

rp_calib_params_t calib_GetParams();
int calib_WriteParams(rp_calib_params_t calib_params);
void calib_SetToZero();

int calib_Reset();

int32_t calib_GetDataMedian(rp_channel_t channel, rp_pinState_t gain);
float calib_GetDataMedianFloat(rp_channel_t channel, rp_pinState_t gain);
int calib_GetDataMinMaxFloat(rp_channel_t channel, rp_pinState_t gain, float* min, float* max);

int calib_setCachedParams();
#endif //__CALIB_H
