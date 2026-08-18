#pragma once
#include <inttypes.h>

namespace CANLib
{
	enum event_type_t : uint8_t 
	{
		EVENTTYPE_NONE, EVENTTYPE_OK, EVENTTYPE_ERROR
	};
	
	void OnSteeringRackEvent(event_type_t type);
};
