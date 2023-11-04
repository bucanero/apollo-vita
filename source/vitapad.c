/*
# based on orbisPad from ORBISDEV Open Source Project.
# Copyright 2010-2020, orbisdev - http://orbisdev.github.io
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>
#include <dbglogger.h>
#include "vitapad.h"

#define LOG dbglogger_log

static VitaPadConfig vitaPadConf;
static int orbispad_initialized = 0;
static uint64_t g_time;


static uint64_t timeInMilliseconds(void)
{
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((uint64_t)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

void vitaPadFinish(void)
{
	int ret;

	if(orbispad_initialized)
	{
		LOG("scePadClose");
	}
	orbispad_initialized=0;

	LOG("ORBISPAD finished");
}

VitaPadConfig *vitaPadGetConf(void)
{
	if(orbispad_initialized)
	{
		return (&vitaPadConf);
	}
	
	return NULL; 
}

static int vitaPadInitConf(void)
{	
	if(orbispad_initialized)
	{
		return orbispad_initialized;
	}

	memset(&vitaPadConf, 0, sizeof(VitaPadConfig));
	
	return 0;
}

unsigned int vitaPadGetCurrentButtonsPressed(void)
{
	return vitaPadConf.buttonsPressed;
}

void vitaPadSetCurrentButtonsPressed(unsigned int buttons)
{
	vitaPadConf.buttonsPressed=buttons;
}

unsigned int vitaPadGetCurrentButtonsReleased(void)
{
	return vitaPadConf.buttonsReleased;
}

void vitaPadSetCurrentButtonsReleased(unsigned int buttons)
{
	vitaPadConf.buttonsReleased=buttons;
}

bool vitaPadGetButtonHold(unsigned int filter)
{
	uint64_t time = timeInMilliseconds();
	uint64_t delta = time - g_time;

	if((vitaPadConf.buttonsHold&filter)==filter && delta > 150)
	{
		g_time = time;
		return 1;
	}

	return 0;
}

bool vitaPadGetButtonPressed(unsigned int filter)
{
	if (!vitaPadConf.crossButtonOK && (filter & (SCE_CTRL_CROSS|SCE_CTRL_CIRCLE)))
		filter ^= (SCE_CTRL_CROSS|SCE_CTRL_CIRCLE);

	if((vitaPadConf.buttonsPressed&filter)==filter)
	{
		vitaPadConf.buttonsPressed ^= filter;
		return 1;
	}

	return 0;
}

bool vitaPadGetButtonReleased(unsigned int filter)
{
 	if((vitaPadConf.buttonsReleased&filter)==filter)
	{
		if(~(vitaPadConf.padDataLast.buttons)&filter)
		{
			return 0;
		}
		return 1;
	}

	return 0;
}

int vitaPadUpdate(void)
{
	int ret;
	unsigned int actualButtons=0;
	unsigned int lastButtons=0;

	memcpy(&vitaPadConf.padDataLast, &vitaPadConf.padDataCurrent, sizeof(SceCtrlData));	
	ret=sceCtrlPeekBufferPositive(0, &vitaPadConf.padDataCurrent, 1);

	if(ret > 0)
	{
		if (vitaPadConf.padDataCurrent.ly < ANALOG_MIN)
			vitaPadConf.padDataCurrent.buttons |= SCE_CTRL_UP;

		if (vitaPadConf.padDataCurrent.ly > ANALOG_MAX)
			vitaPadConf.padDataCurrent.buttons |= SCE_CTRL_DOWN;

		if (vitaPadConf.padDataCurrent.lx < ANALOG_MIN)
			vitaPadConf.padDataCurrent.buttons |= SCE_CTRL_LEFT;

		if (vitaPadConf.padDataCurrent.lx > ANALOG_MAX)
			vitaPadConf.padDataCurrent.buttons |= SCE_CTRL_RIGHT;

		actualButtons=vitaPadConf.padDataCurrent.buttons;
		lastButtons=vitaPadConf.padDataLast.buttons;
		vitaPadConf.buttonsPressed=(actualButtons)&(~lastButtons);
		if(actualButtons!=lastButtons)
		{
			vitaPadConf.buttonsReleased=(~actualButtons)&(lastButtons);
			vitaPadConf.idle=0;
		}
		else
		{
			vitaPadConf.buttonsReleased=0;
			if (actualButtons == 0)
			{
				vitaPadConf.idle++;
			}
		}
		vitaPadConf.buttonsHold=actualButtons&lastButtons;

		return 0;
	}

	return -1;
}

int vitaPadInit(int crossOK)
{
	int ret;

	if(vitaPadInitConf()==1)
	{
		LOG("ORBISPAD already initialized!");
		return orbispad_initialized;
	}

	ret=sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
	if (ret < 0)
	{
		LOG("sceCtrlSetSamplingMode Error 0x%8X", ret);
		return -1;
	}

	vitaPadConf.crossButtonOK = crossOK;
	orbispad_initialized=1;
	g_time = timeInMilliseconds();
	LOG("ORBISPAD initialized: sceCtrlSetSamplingMode return 0x%X", ret);

	return orbispad_initialized;
}
