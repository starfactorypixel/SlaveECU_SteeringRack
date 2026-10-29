#pragma once
#include <inttypes.h>
#include <CUtils.h>

// Служебные типы системы настроек, общие для всех плат. Подключается в начале Config.h платы

namespace Config
{
	/*
		Каждый модуль настроек в Config.h платы оборачивается в section_t<тип модуля, размер модуля> фиксированного размера с резервом.
		Заголовок модуля хранит его полный размер и кол-во байт данных, известных прошивке, записавшей блок.
		По нему при загрузке поля, дописанные в конец модуля новой прошивкой, получают значения по умолчанию, а остальные сохраняются.
	*/
	
	// Заголовок модуля
	structpack section_hdr_t
	{
		// Полный размер модуля, байт
		uint8_t length;
		
		// Кол-во байт данных модуля, известных прошивке, записавшей блок
		uint8_t size;
	};
	
	// Модуль настроек фиксированного размера LENGTH
	template<typename T, uint8_t LENGTH>
	structpack section_t : section_hdr_t, T
	{
		static_assert(sizeof(section_hdr_t) + sizeof(T) <= LENGTH, "Section data exceeds LENGTH");
		
		constexpr section_t() : section_hdr_t{LENGTH, sizeof(T)}, T() {}
		constexpr section_t(const T &def) : section_hdr_t{LENGTH, sizeof(T)}, T(def) {}
		
		// Резерв под новые поля
		uint8_t _fill[LENGTH - sizeof(section_hdr_t) - sizeof(T)] = {};
	};
};
