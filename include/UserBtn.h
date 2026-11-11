#pragma once
#include <inttypes.h>
#include "DrakeButton.h"

namespace UserBtn
{
	void OnEventOnStart(DrakeButton::Event ev, uint32_t ms);
	void OnEventOnRun(DrakeButton::Event ev, uint32_t ms);
	
	DrakeButton btn({GPIOA, GPIO_PIN_8}, OnEventOnStart);
	
	bool need_reset_cfg = false;
	
	
	// При старте маска = только EV_LONG_HOLD, так что сюда приходит только оно.
	void OnEventOnStart(DrakeButton::Event ev, uint32_t ms)
	{
		if(need_reset_cfg == false)
		{
			need_reset_cfg = true;
			Leds::obj.SetOn(Leds::LED_RED);
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
		btn.SetMask(DrakeButton::EV_LONG_HOLD);
		btn.SetTimings(30, 300, 15000, 1000);
		
		// Ждём, пока библиотека определит состояние кнопки, а если она нажата - пока не отпустят.
		do
		{
			btn.Tick(HAL_GetTick());
		} while(btn.IsReady() == false || btn.IsPressed() == true);

		btn.SetMask(DrakeButton::EV_ALL);
		btn.SetTimings(30, 300, 800, 1000);
		btn.SetCallback(OnEventOnRun);
		
		return;
	}
	
	inline void Loop(uint32_t &current_time)
	{
		btn.Tick(current_time);
		
		current_time = HAL_GetTick();
		return;
	}
}
