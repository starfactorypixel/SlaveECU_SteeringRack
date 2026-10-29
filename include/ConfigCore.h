#pragma once
#include <inttypes.h>
#include <string.h>
#include <CUtils.h>
#include "ConfigTypes.h"

// Ядро системы настроек, общее для всех плат. Подключается в конце Config.h платы,
// после объявления CFG_VERSION, NOR_PAGE_OFFSET, NOR_PAGE_COUNT и config_body_t

extern CRC_HandleTypeDef hcrc;

namespace Config
{
	/*
		Под конфиг выделено NOR_PAGE_COUNT страниц по 256 байт (по умолчанию 16 страниц = 4 КБ), очистка выполняется постранично.
		Данные сохраняются последовательно в каждый слот по очереди (при условии что данные изменились при проверки CRC32 всей config_root_t).
		Под данные (слот) может быть выделено более одной страницы (256 байт) при этом последовательная логика сохраняется.
		При чтении перебираются все слоты: каждый слот читается целиком в config_root_t и проверяется (flag_init, cnt_fault, version, crc32).
			Из валидных слотов выбирается слот с максимальным счётчиком записей, - это и есть самая свежая конфигурация.
			Номер страницы актуального конфига сохраняется в global_page_idx. Порядок слотов в памяти значения не имеет.
		Слот с несовпавшей crc32 считается битым (например обрыв питания во время записи), пропускается, а его cnt_fault уменьшается.
			Слот с cnt_fault = 0 пропускается сразу.
		Если валидных слотов нет (первый запуск, смена CFG_VERSION, неисправная память), то в config_root_t загружаются значения по умолчанию
			и записываются в слот 0. Перезагрузка не выполняется, блок работает с тем что лежит в RAM.
		При записи вначале делается проверка на наличие изменений. Для этого высчитывается новый crc32 для config_root_t ( с учётом замещения crc32 поля нулями на момент расчёта ),
			и сравнивается с сохранённым crc32, что даёт флаг о наличии изменения в настройках.
		Затем мы инкрементируем счётчик страниц, который получили при чтении с учётом перехода через границу зоны настроек (NOR_PAGE_COUNT страниц) и размера поля данных, Например:
			Если config_root_t = 1 страницы, то пишем конфиг в   0 >>   1 >> .. >>    15 >>   0 ..
			Если config_root_t = 2 страницы, то пишем конфиг в 0-1 >> 2-3 >> .. >> 14-15 >> 0-1 ..
			Если config_root_t = 3 страницы, то пишем конфиг в 0-2 >> 3-5 >> .. >> 12-14 >> 0-2 .. ( страница 15 не используется )
		После инкрементируем счётчик записей, обновляем поле crc32 для всей config_root_t, очищает страницы и записываем данные.
		Порядок записи страниц не важен: crc32 покрывает всю config_root_t, поэтому недописанный слот не пройдёт проверку,
			а предыдущий слот при записи не затрагивается.
		После загрузки слота модули тела приводятся к текущей прошивке: заголовки модулей и поля, которых не знала прошивка, записавшая блок,
			берутся из CFG_DEFAULTS. Поэтому дописанные в конец модуля поля не требуют смены CFG_VERSION (см. Config.h платы).
			Если что-то изменилось, то crc32 не совпадёт и Loop сохранит обновлённый блок.
	*/

	// Размер страницы NOR
	static constexpr uint16_t NOR_PAGE_SIZE = SPI::flash.NOR_PAGE_SIZE;

	// Интервал проверки и сохранения буфера в память
	static constexpr uint32_t NOR_WRITE_DELAY_MS = 30000;

	// Значение flag_init для инициализированного блока
	static constexpr uint8_t CFG_FLAG_INIT = 0x5A;

	// Значение cnt_fault для инициализированного блока
	static constexpr uint8_t CNT_NOR_FAULT = (1 << 3) - 1;

	// Структура заголовка конфигурации в NOR памяти
	structpack config_header_t
	{
		// Флаги инициализации. CFG_FLAG_INIT - Блок инициализирован, иначе не инициализирован
		uint8_t flag_init;

		// Флаги ошибок. 0x07 - Ошибок нет, 0x00 - Блок не читается (израсходованы попытки чтения)
		uint8_t cnt_fault;

		// Версия формата конфигурации. Блок годен только при точном совпадении с CFG_VERSION
		uint8_t version;

		// Счётчик записей в память. 24 бит хватает с запасом: 64 страницы * 100К циклов = 6.4М < 16.7М
		uint32_t counter : 24;

		// Контрольная сумма всей структуры
		uint32_t crc32;
	};

	// Общая структура всего блока данных
	// Байты лежат строго последовательно (packed), выравнивание на 4 задано только хранилищу config_storage, для HAL_CRC_Calculate
	structpack config_root_t
	{
		// Заголовок
		config_header_t header = {};

		// Блок данных
		config_body_t body;

		// Добивка до размера кратного 4 байтам (для CRC32), может быть нулевой длины
		uint8_t _align[(4 - ((sizeof(config_header_t) + sizeof(config_body_t)) % 4)) % 4] = {};
	};

	// Хранилище config_obj.
	// Значения по умолчанию хранятся во flash МК один раз - в CFG_DEFAULTS. Обычную переменную config_root_t компилятор положит в .data
	// и сохранит во flash МК ещё и копию её начальных значений для стартапа, которая не нужна: Setup() всё равно перезаписывает config_obj.
	// union создаёт только raw, заполненный нулями, конструктор obj не вызывается, поэтому config_obj лежит в .bss без копии во flash МК.
	// Значения заполняются в Setup() из NOR памяти или из CFG_DEFAULTS, до вызова Setup() config_obj заполнен нулями.
	// Выравнивание на 4 задано только для HAL_CRC_Calculate, байты config_root_t лежат строго последовательно
	union config_storage_t
	{
		constexpr config_storage_t() : raw{} {}

		uint8_t raw[sizeof(config_root_t)];
		config_root_t obj;
	} config_storage __attribute__((aligned(4)));

	config_root_t &config_obj = config_storage.obj;
	
	// Вариант без union: проще, но x2 расход flash МК под конфиг (копия начальных значений config_obj для .data + CFG_DEFAULTS)
	// config_root_t config_obj __attribute__((aligned(4)));

	// Значения по умолчанию, лежат во flash МК
	static constexpr config_body_t CFG_DEFAULTS{};


	// Кол-во страниц, которые занимает весь конфиг целиком
	static constexpr uint16_t CFG_PAGES_COUNT = (sizeof(config_root_t) + (NOR_PAGE_SIZE - 1)) / NOR_PAGE_SIZE;

	// Кол-во слотов под конфиг в зоне настроек
	static constexpr uint16_t CFG_SLOTS_COUNT = NOR_PAGE_COUNT / CFG_PAGES_COUNT;

	// Работаем с CRC32, поэтому общий размер структуры должен быть кратен 4 байтам
	static_assert(sizeof(config_root_t) % 4 == 0, "config_root_t size must be divisible by 4 bytes");

	// Нужно минимум 2 слота, иначе обрыв питания во время записи уничтожит единственную копию
	static_assert(CFG_SLOTS_COUNT >= 2, "NOR_PAGE_COUNT must fit at least 2 config slots");

	// Проверяем, что диапазон страниц помещается в доступную NOR Flash
	static_assert(NOR_PAGE_OFFSET + NOR_PAGE_COUNT - 1 <= SPI::flash.NOR_MAX_PAGE, "NOR page range exceeds available flash memory");


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
			global_page_idx = (CFG_SLOTS_COUNT - 1) * CFG_PAGES_COUNT;
		else
			global_page_idx -= CFG_PAGES_COUNT;

		return global_page_idx;
	}

	void _PreWriteConfig()
	{
		auto &header = config_obj.header;
		header.flag_init = CFG_FLAG_INIT;
		header.cnt_fault = CNT_NOR_FAULT;
		header.version = CFG_VERSION;
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
			SPI::flash.WritePage(page, (data_ptr + (i * NOR_PAGE_SIZE)), length);
		}

		return;
	}

	// Приводит модули тела к текущей прошивке: заголовок модуля и поля за пределами известных записавшей блок прошивке
	// берутся из CFG_DEFAULTS, известные поля остаются как прочитаны
	void _UpgradeSections()
	{
		uint8_t *body = (uint8_t *) &config_obj.body;
		const uint8_t *def = (const uint8_t *) &CFG_DEFAULTS;
		for(uint16_t i = 0; i < sizeof(config_body_t); i += def[i])
		{
			uint8_t old_size = body[i + 1];
			uint8_t new_size = def[i + 1];
			uint8_t known = sizeof(section_hdr_t) + ((old_size < new_size) ? old_size : new_size);

			memcpy((body + i), (def + i), sizeof(section_hdr_t));
			memcpy((body + i + known), (def + i + known), (def[i] - known));
		}

		return;
	}

	bool ReadConfig(uint16_t page_idx);
	void _IncReadErrorCounter(uint16_t page_idx);
	
	// Ищет актуальный (с максимальным счётчиком записей) валидный блок конфигурации и загружает его в config_obj
	// Инициализирует память значениями по умолчанию если валидных блоков нет (переписывает блок 0)
	// Возвращает индекс страницы актуального конфига
	uint16_t _FindConfigPage()
	{
		uint16_t cfg_page_idx = 0;
		uint32_t cfg_counter = 0;

		bool is_found = false;
		for(uint16_t page_idx = 0; page_idx + CFG_PAGES_COUNT <= NOR_PAGE_COUNT; page_idx += CFG_PAGES_COUNT)
		{
			bool is_crc_ok = ReadConfig(page_idx);
			auto &header = config_obj.header;

			// Если встретили секцию с неинициализированным блоком, то эта секция нам не нужна
			if(header.flag_init != CFG_FLAG_INIT) continue;

			// Если встретили секцию с израсходованными попытками чтения, то эта секция нам не нужна
			if(header.cnt_fault == 0x00) continue;

			// Если встретили секцию с несовпадением версии, то эта секция нам не нужна
			if(header.version != CFG_VERSION) continue;

			// Если встретили секцию с битыми данными, то уменьшаем её счётчик попыток, и эта секция нам не нужна
			if(is_crc_ok == false)
			{
				_IncReadErrorCounter(page_idx);
				continue;
			}

			// Все проверки пройдены, - найден цельный блок. Запоминаем его, если он свежее найденного ранее
			if(is_found == false || header.counter > cfg_counter)
			{
				cfg_page_idx = page_idx;
				cfg_counter = header.counter;
				is_found = true;
			}
		}

		// В config_obj сейчас последний прочитанный слот, перечитываем найденный
		if(is_found == true && ReadConfig(cfg_page_idx) == false)
			is_found = false;

		// Новые поля модулей получают значения по умолчанию
		if(is_found == true)
			_UpgradeSections();

		// Если первый запуск или аномалия, то загружаем значения по умолчанию и инициализируем первый блок
		if(is_found == false)
		{
			DEBUG_LOG_TOPIC("CFG", "Valid config not found, load defaults\n");

			cfg_page_idx = 0;
			config_obj.header = {};
			memcpy(&config_obj.body, &CFG_DEFAULTS, sizeof(config_body_t));
			WriteConfig(cfg_page_idx);
		}

		global_page_idx = cfg_page_idx;

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

		// Пишем поверх без очистки: в NOR биты меняются только 1 >> 0, поэтому 0x07 >> 0x03 >> 0x01 >> 0x00
		uint32_t addr = ((page_idx + NOR_PAGE_OFFSET) * NOR_PAGE_SIZE) + offsetof(config_header_t, cnt_fault);
		uint8_t count_fault = config_obj.header.cnt_fault >> 1;
		SPI::flash.WriteBytes(addr, &count_fault, 1);

		return;
	}

	bool ReadConfig(uint16_t page_idx)
	{
		// Если флешки нет, то статус читается как 0xFF (вечно busy), WaitReady() вернёт false и ReadPage не тронет буфер.
		// Очищаем заголовок, чтобы данные предыдущего слота не прошли проверку повторно
		config_obj.header = {};

		SPI::flash.ReadPage((page_idx + NOR_PAGE_OFFSET), ((uint8_t *)&config_obj), sizeof(config_root_t));

		return _CheckCRC();
	}

	inline void Setup()
	{
		_FindConfigPage();

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
