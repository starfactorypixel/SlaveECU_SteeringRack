#pragma once
#include <inttypes.h>
#include "DrakeButton.h"

namespace UserBtn
{
	void OnEventOnStart(DrakeButton::Event ev, uint32_t ms);
	void OnEventOnRun(DrakeButton::Event ev, uint32_t ms);
	
	DrakeButton btn({GPIOA, GPIO_PIN_8}, HAL_GetTick, OnEventOnStart);
	
	bool wait_release = false;
	bool need_reset_cfg = false;
	
	
	void OnEventOnStart(DrakeButton::Event ev, uint32_t ms)
	{
		if(ev == DrakeButton::EV_LONG_HOLD)
		{
			need_reset_cfg = true;
			Leds::obj.SetOn(Leds::LED_RED);
		}
		else
		{
			wait_release = false; // CLICK, DOUBLE_CLICK или LONG_RELEASE - кнопку отпустили
		}
		
		return;
	}

	void OnEventOnRun(DrakeButton::Event ev, uint32_t ms)
	{
		switch(ev)
		{
			case DrakeButton::EV_CLICK:
			{
				break;
			}
			default:
			{
				break;
			}
		}

		return;
	}
	
	bool NeedResetCfg()
	{
		return need_reset_cfg;
	}
	
	
	inline void Setup()
	{
		btn.Init();
		if(btn.IsPressed() == true)
		{
			btn.SetTimings(30, 300, 15000, 1000);
			wait_release = true;
			while(wait_release == true)
			{
				btn.Tick();
			}
		}

		btn.SetTimings(30, 300, 800, 1000);
		btn.SetCallback(OnEventOnRun);
		
		return;
	}
	
	inline void Loop(uint32_t &current_time)
	{
		btn.Tick();
		
		current_time = HAL_GetTick();
		return;
	}
}
