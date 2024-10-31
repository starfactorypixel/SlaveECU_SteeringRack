#pragma once

extern TIM_HandleTypeDef htim4;


namespace SteeringRack
{
	
	static constexpr uint16_t PWM_MIN = 800;
	static constexpr uint16_t PWM_MID = 1500;
	static constexpr uint16_t PWM_MAX = 2200;



	struct params_t
	{
		
	};

	enum rack_id_t : uint8_t { RACK_1 = 0, RACK_2 = 1 };





class PIDController {
private:
    float kp, ki, kd;           // Коэффициенты PID
    float integral, previous_error; // Накопление для интегральной части и предыдущее значение ошибки
    //float output_limit_min, output_limit_max; // Ограничения выхода для ШИМ

public:
    PIDController(float p, float i, float d, float min_output, float max_output)
        : kp(p), ki(i), kd(d), integral(0), previous_error(0)/*,
          output_limit_min(min_output), output_limit_max(max_output)*/ {}

    // Функция расчета PID
    float calculate(float setpoint, float measured_value, float dt) {
        float error = setpoint - measured_value;         // Вычисление ошибки
        integral += error * dt;                          // Интегральная часть
        float derivative = (error - previous_error) / dt; // Производная часть
        previous_error = error;

        // Рассчитываем выход
        float output = kp * error + ki * integral + kd * derivative;

        // Ограничиваем выход для ШИМ
        //if (output > output_limit_max) output = output_limit_max;
       // else if (output < output_limit_min) output = output_limit_min;

        return output;
    }
};







class SteeringControl
{
	public:
		SteeringControl(float p, float i, float d, float min_pwm, float mid_pwm, float max_pwm, TIM_HandleTypeDef *htim_pwm, uint32_t pwm_channel) : 
			_pid(p, i, d, min_pwm, max_pwm), _htim(htim_pwm), _channel(pwm_channel), _pwm_min(min_pwm), _pwm_mid(mid_pwm), _pwm_max(max_pwm), 
			_target(0.0f)
		{}

		void SetTarget(float target)
		{
			_target = target;

			return;
		}
		
		// Функция для обновления ПИД и управления ШИМ
		void Update(float measured_value, float dt)
		{
			float pid_output = _pid.calculate(_target, measured_value, dt);
			int32_t value = ((int32_t)(pid_output + 0.5f)) + (int32_t)_pwm_mid;
			uint16_t pwm = clamp(value, _pwm_min, _pwm_max);
			
			DEBUG_LOG_TOPIC("Set pwm", "pid: %f, val: %d, PWM: %d\n", pid_output, value, pwm);
			
			// Преобразуем вывод PID в значение ШИМ и устанавливаем его
			__HAL_TIM_SET_COMPARE(_htim, _channel, pwm);
			
			return;
		}
		
	private:
		
		uint16_t clamp(uint16_t value, uint16_t min, uint16_t max)
		{
			return (value < min) ? min : (value > max) ? max : value;
		}
		
		PIDController _pid;      // Экземпляр PID-контроллера
		TIM_HandleTypeDef *_htim; // Указатель на таймер для управления ШИМ
		uint32_t _channel;       // Канал ШИМ
		float _pwm_min, _pwm_mid, _pwm_max;
		float _target;
};

	SteeringControl steerings[] = 
	{
		{1.0f, 0.1f, 0.01f, PWM_MIN, PWM_MID, PWM_MAX, &htim4, TIM_CHANNEL_1},
		{1.0f, 0.1f, 0.01f, PWM_MIN, PWM_MID, PWM_MAX, &htim4, TIM_CHANNEL_2}
	};
	
	
	void SetTarget(rack_id_t id, float target)
	{
		steerings[id].SetTarget(target);
		
		return;
	}
	
	void Tick(rack_id_t id, float angle, float roll, float dt)
	{
		steerings[id].Update(angle, dt);
		
		return;
	}
	
	
	inline void Setup()
	{
		HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
		HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);


		return;
	}
	
	inline void Loop(uint32_t &current_time)
	{
		
		
		current_time = HAL_GetTick();
		return;
	}
}