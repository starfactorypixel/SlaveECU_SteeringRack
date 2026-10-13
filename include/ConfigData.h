#pragma once
#include <inttypes.h>

namespace Config
{
	struct __attribute__((packed)) config_body_t
	{
		struct
		{
			bool maintenance = true;	// Режим настройки
			uint16_t base_id = 0x01A0;	// Базовый CAN ID
			uint16_t vref = 3300;		// Опорное напряжение ADC, мВ
			int16_t correction = 0;		// Коррекция ADC измерений, 0.01%

			uint8_t _fill[32 - 7];
		} system;
		
		struct
		{
			bool enable = true;
			bool invert = false;
			int16_t offset = 0;
		} rack1;
		struct
		{
			bool enable = true;
			bool invert = true;
			int16_t offset = 0;
		} rack2;

		//uint8_t _padding[2];
	};
};
