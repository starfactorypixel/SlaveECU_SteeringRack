#pragma once
#include <inttypes.h>
#include <DrakePinD.hpp>
#include <SteeringAngleSensor.h>

namespace CAN_SPI
{
	void OnSteeringAngleSensorError(uint8_t id, SteeringAngleSensorBase::error_t code);
	
	SteeringAngleSensor sensor1(SteeringRack::RACK_1, OnSteeringAngleSensorError);
	SteeringAngleSensor sensor2(SteeringRack::RACK_2, OnSteeringAngleSensorError);
	
	void OnSteeringAngleSensorError(uint8_t id, SteeringAngleSensorBase::error_t code)
	{
		Logger.Printf("Sensor %d: error code: %d", id, code).PrintNewLine();
		SteeringRack::OnErrorSensor((SteeringRack::rack_id_t)id, code);
		
		return;
	}
	
	void CAN_RX(uint8_t can_port, uint32_t address, uint8_t *data, uint8_t length)
	{
		bool result;
		uint32_t time;

		DEBUG_LOG_TOPIC("ExCAN RX", "Port: %d, Addr: %04X, Data(%d): ", can_port, address, length);
		DEBUG_LOG_ARRAY_HEX("ExCAN RX", data, length);
		DEBUG_LOG_NEW_LINE();
		
		switch(can_port)
		{
			case 1:
			{
				time = HAL_GetTick();
				
				result = sensor1.PutPacket(time, address, data, length);
				if(result == true)
				{
					SteeringRack::OnDataSensor( SteeringRack::RACK_1, sensor1.data_float->angle, sensor1.data_float->roll, sensor1.data_float->dt );

					//DEBUG_LOG_TOPIC("ExCAN RX", "Port: %d, Addr: %04X, Angle: %+05d, Roll: %+05d, Err: %02d\n", id, address, sensor1.data_int->angle, sensor1.data_int->roll, sensor1.data_int->error);
				}
				
				break;
			}
			case 2:
			{
				time = HAL_GetTick();
				
				result = sensor2.PutPacket(time, address, data, length);
				if(result == true)
				{
					SteeringRack::OnDataSensor( SteeringRack::RACK_2, sensor2.data_float->angle, sensor2.data_float->roll, sensor2.data_float->dt );
				}
				
				break;
			}
		}
		
		return;
	}	



	// Управления питанием 12V на внешние CAN устройство
	DrakePinD Vcc1En({GPIOA, GPIO_PIN_3}, DrakePin::Output, DrakePin::Low);
	DrakePinD Vcc2En({GPIOA, GPIO_PIN_4}, DrakePin::Output, DrakePin::Low);

	// Управление сном CAN передатчика
	DrakePinD Can1Stby({GPIOB, GPIO_PIN_2}, DrakePin::OutputOpenDrain, DrakePin::High);
	DrakePinD Can2Stby({GPIOB, GPIO_PIN_8}, DrakePin::OutputOpenDrain, DrakePin::High);




	inline void Setup()
	{
		Vcc1En.Init();
		Vcc2En.Init();
		Can1Stby.Init();
		Can2Stby.Init();
		// Реализовать управление
		
		SPI::can1.begin(8000000, 500000, [](uint32_t address, uint8_t *data, uint8_t length){ CAN_RX(1, address, data, length); });
		SPI::can2.begin(8000000, 500000, [](uint32_t address, uint8_t *data, uint8_t length){ CAN_RX(2, address, data, length); });



		sensor1.SetOffset( Config::Obj().rack1.offset );
		sensor1.SetInvert( Config::Obj().rack1.invert );
		sensor2.SetOffset( Config::Obj().rack2.offset );
		sensor2.SetInvert( Config::Obj().rack2.invert );

		Vcc1En.On();
		Vcc2En.On();
		Can1Stby.Off();
		Can2Stby.Off();

		
		return;
	}
	
	inline void Loop(uint32_t &current_time)
	{
		
		sensor1.Tick(current_time);
		sensor2.Tick(current_time);
		
		current_time = HAL_GetTick();
		
		return;
	}
};
