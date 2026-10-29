#pragma once
#include "ConfigTypes.h"

namespace Config
{
	// Версия структуры конфигурации
	static constexpr uint8_t CFG_VERSION = 5;

	// Смещение начала блока с конфигом, в страницах
	static constexpr uint16_t NOR_PAGE_OFFSET = 0;

	// Кол-во страниц, выделенные под настройки для поочередной записи
	static constexpr uint16_t NOR_PAGE_COUNT = 16;
	
	/*
		Каждый модуль - section_t<тип модуля, размер модуля>, config_body_t состоит только из section_t.
		Новые поля дописываются строго в конец типа модуля, CFG_VERSION при этом не меняется.
		Вставка поля в середину, изменение типа или удаление поля, изменение размера или порядка модулей - только со сменой CFG_VERSION.
		Значения по умолчанию задаются в типе модуля, а для отдельного экземпляра - перечислением по порядку: { {поле1, поле2, ..} },
			не перечисленные поля в конце берутся из типа модуля.
	*/
	
	structpack system_t
	{
		bool maintenance = true;	// Режим настройки
		uint16_t base_id = 0x01A0;	// Базовый CAN ID
		uint16_t vref = 3300;		// Опорное напряжение ADC, мВ
		int16_t correction = 0;		// Коррекция ADC измерений, 0.01%
	};
	
	structpack rack_t
	{
		bool enable = true;
		bool invert = false;
		int16_t offset = 0;
	};
	
	structpack config_body_t
	{
		section_t<system_t, 32> system;
		section_t<rack_t, 16> rack1;
		section_t<rack_t, 16> rack2 { {true, true} };
	};
};

#include "ConfigCore.h"
