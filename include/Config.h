#pragma once
#include <inttypes.h>
#include "ConfigData.h"

extern CRC_HandleTypeDef hcrc;

namespace Config
{
	/*
		Под конфиг выделен 1 сектор = 4 КБ или 16 страниц по 256 байт.
		Данные сохраняются последовательно в каждый сектор по очереди ( при условии что данные изменились, например проверка по CRC32 всей config_root_t ).
		Под данные может быть выделено более одной страницы (256 байт) при этом последовательная логика сохраняется.
		При чтении вначале читается заголовок и сравнивается со следующим заголовком. Таким образом находится страница самой свежей конфигурации.
		Затем читается весь блок данных в структуру config_root_t. При этом сохраняется номер страницы актуального конфига и подсчитывается CRC config_root_t, 
			или чтобы не создавать иной алгоритм используется поле crc32.
		При записи вначале делается проверка на наличие изменений. Для этого высчитывается новый crc32 для config_root_t ( с учётом замещения crc32 поля нулями на момент расчёта ), 
			и сравнивается с сохранённым crc32, что даёт флаг о наличии изменения в настройках.
		Затем мы инструментируем счётчик страниц, который получили при чтении с учётом перехода через границу зоны настроек (16 страниц) и размера поля данных, Например:
			Если config_root_t = 1 страницы, то пишем конфиг в   0 >>   1 >> .. >>    15 >>   0 ..
			Если config_root_t = 2 страницы, то пишем конфиг в 0-1 >> 2-3 >> .. >> 14-15 >> 0-1 ..
			Если config_root_t = 3 страницы, то пишем конфиг в 0-2 >> 3-5 >> .. >> 12-14 >> 0-2 ..
		После инструментируем счётчик записей, обновляем поле crc32 для всей config_root_t, очищает страницы и записываем данные.
	*/

	static constexpr uint8_t CFG_VERSION = 4;

	// Смещение начала блока с конфигом, в страницах
	static constexpr uint16_t NOR_PAGE_OFFSET = 0;

	// Размер страницы NOR
	static constexpr uint16_t NOR_PAGE_SIZE = SPI::flash.NOR_PAGE_SIZE;

	// Кол-во страниц, выделенные под настройки для поочередной записи
	static constexpr uint16_t NOR_PAGE_COUNT = 16;

	// Интервал проверки и сохранения буфера в память
	static constexpr uint32_t NOR_WRITE_DELAY_MS = 30000;
	
	// Значние flag_init для инициализированного блока
	static constexpr uint8_t CFG_FLAG_INIT = 0x77;

	// Значние cnt_fault для инициализированного блока
	static constexpr uint8_t CNT_NOR_FAULT = (1 << 3) - 1;
	
	// Структура заголовка конфигурации в NOR памяти
	struct __attribute__((packed)) config_header_t
	{
		// Флаги инициализации. 0xFF - Блок не инициализирован, 0x00 - Инициализирован
		uint8_t flag_init;

		// Флаги ошибок. 0x07 - Ошибок нет, 0x00 - Блок не читается
		uint8_t cnt_fault;
		
		// Версия формата заголовка, а так-же флаг наличия записи в блоке (если 0x00 или 0x1F, то считаем что блок не инициализирован)
		uint8_t verison;
		
		// Счётчик записей в память
		uint32_t counter : 24;

		// Контрольная сумма всей структуры
		uint32_t crc32;
	};

	// Общая структура всего блока данных
	struct __attribute__((packed)) config_root_t
	{
		// Заголовок
		config_header_t header = {};
		
		// Блок данных
		config_body_t body;
	} config_obj;
	
	
	// Кол-во страниц, которые занимает весь конфиг целиком
	static constexpr uint16_t CFG_PAGES_COUNT = (sizeof(config_root_t) + (NOR_PAGE_SIZE - 1)) / NOR_PAGE_SIZE;

	
	// Работаем с CRC32, поэтому для упрощения общий массив данных должен быть кратен 4
	static_assert(sizeof(config_root_t) % 4 == 0, "config_root_t must be a multiple of 4!");

	// Проверяем что смещение + кол-во сраниц слезает в память
	static_assert(NOR_PAGE_OFFSET + NOR_PAGE_COUNT - 1 <= SPI::flash.NOR_MAX_PAGE, "config_root_t must be a multiple of 4!");
	
	
	// Индекс последний страницы с актуальным конфигом, без учёта NOR_PAGE_OFFSET
	uint16_t global_page_idx = 0;
	
	
	config_body_t &Obj()
	{
		return config_obj.body;
	}
	
	
	uint16_t _GetNextPageIdxToWriteConfig()
	{
		global_page_idx += CFG_PAGES_COUNT;
		if(global_page_idx + CFG_PAGES_COUNT > NOR_PAGE_COUNT)
			global_page_idx = 0;
		
		return global_page_idx;
	}
	
	uint16_t _GetPrevPageIdxToReadConfig()
	{
		if(global_page_idx < CFG_PAGES_COUNT)
			global_page_idx = NOR_PAGE_COUNT - CFG_PAGES_COUNT;
		else
			global_page_idx -= CFG_PAGES_COUNT;
		
		return global_page_idx;
	}
	
	void _PreWriteConfig()
	{
		auto &header = config_obj.header;
		header.flag_init = CFG_FLAG_INIT;
		header.cnt_fault = CNT_NOR_FAULT;
		header.verison = CFG_VERSION;
		header.counter += 1U;
		header.crc32 = 0x00000000;
		header.crc32 = HAL_CRC_Calculate(&hcrc, ((uint32_t *)&config_obj), sizeof(config_root_t) / 4);
		
		return;
	}
	
	void WriteConfig(uint16_t page_idx)
	{
		DEBUG_LOG_TOPIC("CFG", "WriteConfig: page:%d\n", page_idx);

		_PreWriteConfig();
		
		uint8_t *data_ptr = (uint8_t *) &config_obj;
		for(uint16_t i = 0; i < CFG_PAGES_COUNT; ++i)
		{
			uint16_t page = page_idx + NOR_PAGE_OFFSET + i;
			uint16_t remaining = sizeof(config_root_t) - (i * NOR_PAGE_SIZE);
			uint16_t length = (remaining > NOR_PAGE_SIZE) ? NOR_PAGE_SIZE : remaining;
			
			SPI::flash.ErasePage(page);
			SPI::flash.WaitReady();
			SPI::flash.WritePage(page, (data_ptr + (i * NOR_PAGE_SIZE)), length);
		}
		
		return;
	}

	// Ищет актульный (последний) по счётчику записей блок конфигурации
	// Инициализирует память если требуется (переписывает блок 0)
	// Возвращает индекс страницы откуда для чтения
	uint16_t _FindConfigPage()
	{
		uint16_t cfg_page_idx = 0;
		
		config_header_t headers[2] = {};
		bool is_found = false;
		for(uint8_t page_idx = 0; page_idx < NOR_PAGE_COUNT; page_idx += CFG_PAGES_COUNT)
		{
			SPI::flash.ReadPage((page_idx + NOR_PAGE_OFFSET), ((uint8_t *)&headers[1]), sizeof(config_header_t));

			// Если встретили секцию с неинициализированным блоком, то эта и следующие секции нам не нужны
			if(headers[1].flag_init != CFG_FLAG_INIT) break;

			// Если встретили секцию с израсходованными попытками чтения, то эта секция нам не нужна
			if(headers[1].cnt_fault == 0x00) continue;
			
			// Если встретили секцию с несовпадением версии или не инициализированным счётчиком, то эта и следующие секции нам не нужны
			if(headers[1].verison != CFG_VERSION) break;

			// Если встретили секцию с счётчиков меньше предыдущего, то эта и следующие секции нам не нужны
			if(headers[1].counter < headers[0].counter) break;

			// Все проверки пройдены, - найден цельный блок
			cfg_page_idx = page_idx;
			is_found = true;
			
			// Копируем текущий заголовок в предыдущий
			headers[0] = headers[1];
		}
		
		// Если первый запуск или аномалия, то инициализируем первый блок
		if(is_found == false)
			WriteConfig(cfg_page_idx);
		
		return cfg_page_idx;
	}
	
	bool _CheckCRC()
	{
		auto &header = config_obj.header;
		uint32_t old_crc32 = header.crc32;
		header.crc32 = 0x00000000;
		uint32_t new_crc32 = HAL_CRC_Calculate(&hcrc, ((uint32_t *)&config_obj), sizeof(config_root_t) / 4);
		header.crc32 = old_crc32;
		
		return (old_crc32 == new_crc32);
	}
	
	void _IncReadErrorCounter(uint16_t page_idx)
	{
		DEBUG_LOG_TOPIC("CFG", "IncReadErrorCounter: page:%d\n", page_idx);
		
		uint32_t addr = ((page_idx + NOR_PAGE_OFFSET) * NOR_PAGE_SIZE) + offsetof(config_root_t, header.cnt_fault);
		uint8_t count_fault = config_obj.header.cnt_fault >> 1;
		SPI::flash.WriteBytes(addr, &count_fault, 1);
		SPI::flash.WaitReady();
		
		return;
	}
	
	bool ReadConfig(uint16_t page_idx)
	{
		SPI::flash.ReadPage((page_idx + NOR_PAGE_OFFSET), ((uint8_t *)&config_obj), sizeof(config_root_t));
		
		return _CheckCRC();
	}
	
	
	inline void Setup()
	{
		uint16_t page_idx = _FindConfigPage();
		if(ReadConfig(page_idx) == false)
		{
			_IncReadErrorCounter(page_idx);
			HAL_NVIC_SystemReset();
		}
		
		return;
	}
	
	inline void Loop(uint32_t &current_time)
	{
		static uint32_t last_save_time = 0;
		if(current_time - last_save_time > NOR_WRITE_DELAY_MS)
		{
			last_save_time = current_time;
			
			if(_CheckCRC() == false)
			{
				uint16_t page = _GetNextPageIdxToWriteConfig();
				WriteConfig(page);
			}
		}
		
		current_time = HAL_GetTick();
		return;
	}
};
