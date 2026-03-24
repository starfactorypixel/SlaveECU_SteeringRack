#pragma once
#include <inttypes.h>

namespace Config
{
	struct __attribute__((packed)) config_body_t
	{
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
	};
};
