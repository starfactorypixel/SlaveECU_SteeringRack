#pragma once
#include "SteeringPWMControlClass.h"
#include "SteeringAngleSensorBase.h"

extern TIM_HandleTypeDef htim4;

namespace SteeringRack
{
	
	static constexpr uint16_t PWM_MIN = 900;
	static constexpr uint16_t PWM_MID = 1500;
	static constexpr uint16_t PWM_MAX = 2100;

	static constexpr float PID_KP = 5.0f;
	static constexpr float PID_KI = 0.5f;
	static constexpr float PID_KD = 0.01f;

	static constexpr float ANGLE_MID = 0.0f;

	

	enum rack_id_t : uint8_t
	{
		RACK_1 = 0, 		// Передняя рейка
		RACK_2 = 1,			// Задняя рейка
	};

	enum steering_mode_t : uint8_t
	{
		STEERING_MODE_NONE = 0,			// Полностью отключенное электронное управление поворотами
		STEERING_MODE_STRAIGHT = 1,		// Задняя ось выравнивается в нулевое положение. Положение передней оси игнорируется
		STEERING_MODE_REVERSE = 2,		// Задняя ось выравнивается на обратный угол относительно передней оси
		STEERING_MODE_MIRROR = 3,		// Задняя ось выравнивается на тот же угол равный передней оси
		STEERING_MODE_LOCK = 4,			// Задняя ось в режиме фиксации выставленного угла (устанавливается последнее фактическое значение задней оси)
		STEERING_MODE_REMOTE = 128		// Режим удалённого управления
	};
	
	steering_mode_t mode = STEERING_MODE_NONE;
	float target = 0.0f;

	float angleMaster = 0.0f;
	float angleSlave = 0.0f;
	int16_t angleMasterInt = 0;
	int16_t angleSlaveInt = 0;

	bool isSensorsCalculated = false;
	SteeringAngleSensorBase::error_t lastSensorErrorCode = SteeringAngleSensorBase::ERROR_NONE;

	SteeringPWMControl steerings[] = 
	{
		{PID_KP, PID_KI, PID_KD, PWM_MIN, PWM_MID, PWM_MAX, &htim4, TIM_CHANNEL_1},
		{PID_KP, PID_KI, PID_KD, PWM_MIN, PWM_MID, PWM_MAX, &htim4, TIM_CHANNEL_2}
	};
	


	void ChangeMode(steering_mode_t mode);



	// Получение актуального значения с датчиков
	void OnDataSensor(rack_id_t id, float angle, float roll, float dt)
	{
		// angleMasterInt, angleSlaveInt - временный костыль чтобы были актуальные переменные для отправки в CAN

		if(id == RACK_1)
		{
			angleMaster = angle;
			angleMasterInt = (int16_t)(angle * 10);
		} else {
			angleSlave = angle;
			angleSlaveInt = (int16_t)(angle * 10);
		}
		steerings[id].Update(angle, dt);
		isSensorsCalculated = true;
		
		return;
	}
	
	// Получение кода ошибки с датчиков
	void OnErrorSensor(rack_id_t id, SteeringAngleSensorBase::error_t code)
	{
		// Логическая ошибка. При ошибке с датчика, мы вызываем ChangeMode(STEERING_MODE_NONE), потом когда ошибка снимается вышываем ChangeMode(mode);
		// т.е. возобновляем режим, но при если в состоянии ошибки в loop() switch(mode) продолжает выполнять старый режим.
		// Нужно переработать логику ативного режима, его сброса и отправки состояния и ошибок

		if(code > 0)
		{
			lastSensorErrorCode = code;
			ChangeMode(STEERING_MODE_NONE);

		}
		else
		{
			if(lastSensorErrorCode == SteeringAngleSensorBase::ERROR_LOST)
			{
				ChangeMode(STEERING_MODE_NONE);
			}
			else
			{
				ChangeMode(mode);
			}
		}
	}
	
	// Установить новый режим работы реек
	void ChangeMode(steering_mode_t mode)
	{
		switch(mode)
		{
			case STEERING_MODE_NONE:
			{
				steerings[RACK_1].SetStopPWM();
				steerings[RACK_2].SetStopPWM();
				break;
			}
			case STEERING_MODE_STRAIGHT:
			{
				steerings[RACK_1].SetStopPWM();
				steerings[RACK_2].SetStartPWM();
				steerings[RACK_2].SetTarget(ANGLE_MID);
				break;
			}
			case STEERING_MODE_REVERSE:
			{
				steerings[RACK_1].SetStopPWM();
				steerings[RACK_2].SetStartPWM();
				steerings[RACK_2].SetTarget(ANGLE_MID - angleMaster);
				break;
			}
			case STEERING_MODE_MIRROR:
			{
				steerings[RACK_1].SetStopPWM();
				steerings[RACK_2].SetStartPWM();
				steerings[RACK_2].SetTarget(angleMaster);
				break;
			}
			case STEERING_MODE_LOCK:
			{
				steerings[RACK_1].SetStopPWM();
				steerings[RACK_2].SetStartPWM();
				steerings[RACK_2].SetTarget(angleMaster);
				break;
			}
			case STEERING_MODE_REMOTE:
			{
				//пока не управляем
				steerings[RACK_1].SetStopPWM();
				steerings[RACK_2].SetStopPWM();
				break;
			}
			default:
			{
				steerings[RACK_1].SetStopPWM();
				steerings[RACK_2].SetStopPWM();
				break;
			}
		}
		
		CANLib::OnSteeringRackEvent(CANLib::EVENTTYPE_OK);
		
		return;
	}
	
	// Управление режимом через CAN
	void ChangeMode(uint8_t fId, uint8_t new_mode)
	{
		if(fId != 1) return;
		mode = (steering_mode_t) new_mode;
		return ChangeMode(mode);
	}
	
	// Управление целевым углом поворота через CAN
	void ChangeTarget(uint8_t fId, int16_t target)
	{
		if(fId != 1) return;
		return;
	}
	
	// Запрос режима через CAN
	uint8_t GetMode()
	{
		return mode;
	}
	
	// Запрос целевого угла поворота через CAN
	int16_t GetTarget()
	{
		return 0;
	}
	
	
	inline void Setup()
	{
		HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
		HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
		
		return;
	}
	
	inline void Loop(uint32_t &current_time)
	{
		if(isSensorsCalculated == true)
		{
			isSensorsCalculated = false;
			
			switch(mode)
			{
				case STEERING_MODE_NONE:
				{
					break;
				}
				case STEERING_MODE_STRAIGHT:
				{
					break;
				}
				case STEERING_MODE_REVERSE:
				{
					steerings[RACK_2].SetTarget(ANGLE_MID - angleMaster);
					break;
				}
				case STEERING_MODE_MIRROR:
				{
					steerings[RACK_2].SetTarget(angleMaster);
					break;
				}
				case STEERING_MODE_LOCK:
				{
					break;
				}
				case STEERING_MODE_REMOTE:
				{
					break;
				}
				default:
				{
					steerings[RACK_1].SetStopPWM();
					steerings[RACK_2].SetStopPWM();				
					break;
				}
			}
		}
		
		current_time = HAL_GetTick();
		
		return;
	}
}
