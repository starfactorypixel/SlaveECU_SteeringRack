#pragma once
#include <ConstantLibrary.h>

namespace About
{
	static constexpr char name[] = "SteeringRackECU";
	static constexpr char desc[] = "Steering Rack control board for Pixel project";
	static constexpr char board_type = Consts::BOARD_TYPE_STEERING_RACK;		// 5 bits
	static constexpr char board_ver = 3;		// 3 bits
	static constexpr char soft_ver = 3;			// 6 bits
	static constexpr char can_ver = 1;			// 2 bits
	static constexpr char git[] = "https://github.com/starfactorypixel/SlaveECU_SteeringRack";
	
	inline void Setup()
	{
		Logger.PrintNewLine();
		Logger.PrintTopic("INFO").Printf("%s, board:%d, soft:%d, can:%d", name, board_ver, soft_ver, can_ver).PrintNewLine();
		Logger.PrintTopic("INFO").Printf("Desc: %s", desc).PrintNewLine();
		Logger.PrintTopic("INFO").Printf("Build: %s %s", __DATE__, __TIME__).PrintNewLine();
		Logger.PrintTopic("INFO").Printf("GitHub: %s", git).PrintNewLine();
		
		// Please don't delete this. It's very important to me.
		Logger.PrintTopic("INFO").Printf("In memory of my beloved Grandfather. 22.03.1942 - 06.12.2023").PrintNewLine();
		
		Logger.PrintTopic("READY").PrintNewLine();
		
		return;
	}
	
	inline void Loop(uint32_t &current_time)
	{
		return;
	}
}
