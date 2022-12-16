/*
# based on orbisPad from ORBISDEV Open Source Project.
# Copyright 2010-2020, orbisdev - http://orbisdev.github.io
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <psp2/ctrl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ANALOG_CENTER       0x78
#define ANALOG_THRESHOLD    0x68
#define ANALOG_MIN          (ANALOG_CENTER - ANALOG_THRESHOLD)
#define ANALOG_MAX          (ANALOG_CENTER + ANALOG_THRESHOLD)


typedef struct VitaPadConfig
{
	SceCtrlData padDataCurrent;
	SceCtrlData padDataLast;
	unsigned int buttonsPressed;
	unsigned int buttonsReleased;
	unsigned int buttonsHold;
	unsigned int idle;
} VitaPadConfig;

int vitaPadInit();
void vitaPadFinish();
VitaPadConfig *vitaPadGetConf();
bool vitaPadGetButtonHold(unsigned int filter);
bool vitaPadGetButtonPressed(unsigned int filter);
bool vitaPadGetButtonReleased(unsigned int filter);
unsigned int vitaPadGetCurrentButtonsPressed();
unsigned int vitaPadGetCurrentButtonsReleased();
void vitaPadSetCurrentButtonsPressed(unsigned int buttons);
void vitaPadSetCurrentButtonsReleased(unsigned int buttons);
int vitaPadUpdate();

#ifdef __cplusplus
}
#endif
