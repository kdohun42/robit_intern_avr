#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>
#include "i2c_lcd.h"

/* 주행 상태값 */
int HORIZON_COUNT = 0;
int Black_zone = 0;
volatile uint32_t system_ms = 0;
uint32_t last_lcd_update_ms = 0;
uint32_t run_start_ms = 0;
uint8_t zone3_delay_active = 0;
uint32_t zone3_delay_start_ms = 0;
uint32_t bm_calibration_start_ms = 0;
uint8_t blackmap_align_done = 0;
uint8_t line_mode = 0;
uint8_t zone_change_done = 0;

/* 1ms 타이머 설정 */
void Timer0_Init(void)
{
	TCNT0 = 0;
	OCR0 = 249;
	TCCR0 = (1 << WGM01) | (1 << CS02);
	TIMSK |= (1 << OCIE0);
}

/* 현재 시간 가져오기 */
uint32_t Get_Millis(void)
{
	uint32_t value;
	uint8_t old_sreg;
	old_sreg = SREG;
	cli();
	value = system_ms;
	SREG = old_sreg;
	return value;
}

/* 첫 가로선 검사 시간 확인 */
uint8_t First_Horizon_Time_Ready(void)
{
	uint32_t elapsed;
	elapsed = Get_Millis() - run_start_ms;
	if (elapsed >= 11000UL)
	{
		return 1;
	}
	return 0;
}

/* 주행 시간과 가로선 개수 출력 */
void LCD_Time_Print(void)
{
	char buffer[21];
	uint32_t elapsed_ms;
	uint32_t sec;
	uint16_t decimal;
	elapsed_ms = Get_Millis() - run_start_ms;
	sec = elapsed_ms / 1000;
	decimal = (elapsed_ms % 1000) / 100;
	i2c_lcd_goto_xy(0, 0);
	sprintf(buffer, "TIME:%lu.%u sec    ", (unsigned long)sec, (unsigned int)decimal);
	i2c_lcd_string(buffer);
	i2c_lcd_goto_xy(1, 0);
	sprintf(buffer, "COUNT:%d          ", HORIZON_COUNT);
	i2c_lcd_string(buffer);
}

/* IR 실제 순서 PF1 PF2 PF3 PF6 PF5 PF4 */
uint8_t sensor_channel[6] = {1, 2, 3, 6, 5, 4};
	
/* 왼쪽은 음수 오른쪽은 양수 */
int16_t sensor_weight[6] = {-2500, -1500, -500, 500, 1500, 2500};
/* 검은 배경 흰 선에서 사용할 센서별 임계값 */
uint16_t white_map_threshold[6] = {600, 500, 300, 300, 500, 600};
	
/*========================= 캘리브레이션 관련 변수 ==================================*/
uint16_t sensor_raw[6];
uint16_t sensor_min[6];
uint16_t sensor_max[6];
uint16_t sensor_normalized[6];
uint16_t white_cal_min[6];
uint16_t white_cal_max[6];
uint16_t black_cal_min[6];
uint16_t black_cal_max[6];
uint8_t white_cal_done = 0;
uint8_t black_cal_done = 0;


/*=========================== 주행 관련 변수 ===================================*/
uint8_t blackmap_normal_mode = 0;

uint8_t calibration_done = 0;

uint8_t running = 0;

int8_t last_direction = 0;

int blackmap_turn_right_count = 0;
int blackmap_turn_left_count = 0;

uint8_t cross_seen = 0;
uint8_t cross_checking = 0;
uint8_t cross_timer = 0;
uint8_t left_seen = 0;
uint8_t right_seen = 0;
uint8_t wide_seen = 0;
uint8_t cross_latched = 0;
uint8_t IR_COUNT(uint8_t mask);
uint8_t HORIZON_COUNTER(uint8_t sensor_mask);


/*=================================== 함수 선언 ======================================*/
void Read_All_Sensors(void);
void PSD_Wait_Until_Clear(void);
void ZONE_2(void);
void ZONE_3(void);
void parking(void);
uint16_t Get_Line_Value(uint8_t index);
void blackmap_calibration();
void Turn_90_Left(void);
void Turn_90_Right(void);
void Turn_120_Right(void);
uint16_t Get_Line_Threshold(uint8_t index);
void Black_Map_Line_Control(void);
void Black_Map_Search_Line(void);
void PSD_Forward_Until_Target(void);
void LCD_BlackMap_Count_Print(void);
void Save_White_Calibration(void);
void Save_Black_Calibration(void);
void Load_White_Calibration(void);
void Load_Black_Calibration(void);
void Black_Map_Normal_Line_Control(void);

/* JTAG 끄고 PF 포트 사용 */
void JTAG_Disable(void)
{
	uint8_t old_sreg;
	uint8_t value;
	old_sreg = SREG;
	cli();
	value = MCUCSR | (1 << JTD);
	MCUCSR = value;
	MCUCSR = value;
	SREG = old_sreg;
}

/* ADC 초기 설정 */
void ADC_Init(void)
{
	DDRF &= ~((1 << PF0) | (1 << PF1) | (1 << PF2) | (1 << PF3) | (1 << PF4) | (1 << PF5) | (1 << PF6));
	PORTF &= ~((1 << PF0) | (1 << PF1) | (1 << PF2) | (1 << PF3) | (1 << PF4) | (1 << PF5) | (1 << PF6));
	ADMUX = (1 << REFS0);
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

/* ADC 한 번 변환 */
uint16_t ADC_Convert_Once(void)
{
	uint8_t low;
	uint8_t high;
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC))
	{
		;
	}
	low = ADCL;
	high = ADCH;
	return ((uint16_t)high << 8) | low;
}

/* 원하는 ADC 채널 읽기 */
uint16_t ADC_Read(uint8_t channel)
{
	ADMUX = (1 << REFS0) | (channel & 0x07);
	ADC_Convert_Once();
	return ADC_Convert_Once();
}

/* PSD 값 평균내서 읽기 */
uint16_t PSD_Read(void)
{
	uint32_t sum = 0;
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		sum += ADC_Read(0);
	}
	return (uint16_t)(sum / 8);
}

/* LED 초기화 */
void LED_Init(void)
{
	DDRA |= (1 << PA0) | (1 << PA1) | (1 << PA2) | (1 << PA3) | (1 << PA4) | (1 << PA5);
	PORTA |= (1 << PA0) | (1 << PA1) | (1 << PA2) | (1 << PA3) | (1 << PA4) | (1 << PA5);
}

/* LED 켜기 */
void LED_On(uint8_t led)
{
	PORTA &= ~(1 << led);
}

/* LED 끄기 */
void LED_Off(uint8_t led)
{
	PORTA |= (1 << led);
}

/* LED 전부 끄기 */
void LED_All_Off(void)
{
	uint8_t i;
	for (i = 0; i < 6; i++)
	{
		LED_Off(i);
	}
}

/* IR 인식 상태를 LED로 표시 */
void LED_Update(void)
{
	uint8_t i;
	for (i = 0; i < 6; i++)
	{
		if (Get_Line_Value(i) >= Get_Line_Threshold(i))
		{
			LED_On(i);
		}
		else
		{
			LED_Off(i);
		}
	}
}

/* 스위치 입력 설정 */
void Switch_Init(void)
{
	DDRE &= ~((1 << PE4) | (1 << PE5));
	PORTE &= ~((1 << PE4) | (1 << PE5));
}

/* 스위치 눌림 확인 */
uint8_t Switch_Pressed(uint8_t pin)
{
	if (!(PINE & (1 << pin)))
	{
		_delay_ms(20);
		if (!(PINE & (1 << pin)))
		{
			while (!(PINE & (1 << pin)))
			{
				;
			}
			_delay_ms(20);
			return 1;
		}
	}
	return 0;
}

/* 모터 PWM 설정 */
void PWM_Init(void)
{
	DDRB |= (1 << PB5) | (1 << PB6);
	TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM10);
	TCCR1B = (1 << CS10);
	OCR1A = 0;
	OCR1B = 0;
}

/* 모터 제어핀 설정 */
void Motor_Init(void)
{
	DDRB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3);
	PORTB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3));
}

/* 왼쪽 모터 속도와 방향 제어 */
void Motor_Left(int16_t speed)
{
	/* 최대 속도 255로 제한 */
	if (speed > 255)
	{
		speed = 255;
	}
	/* 최대 역방향 속도 -255로 제한 */
	if (speed < -255)
	{
		speed = -255;
	}
	/* 양수면 왼쪽 모터 정방향 */
	if (speed > 0)
	{
		PORTB |= (1 << PB0);
		PORTB &= ~(1 << PB1);
		OCR1A = (uint8_t)speed;
	}
	/* 음수면 왼쪽 모터 역방향 */
	else if (speed < 0)
	{
		PORTB &= ~(1 << PB0);
		PORTB |= (1 << PB1);
		OCR1A = (uint8_t)(-speed);
	}
	else
	{
		OCR1A = 0;
		PORTB &= ~(1 << PB0);
		PORTB &= ~(1 << PB1);
	}
}

/* 오른쪽 모터 속도와 방향 제어 */
void Motor_Right(int16_t speed)
{
	if (speed > 255)
	{
		speed = 255;
	}
	if (speed < -255)
	{
		speed = -255;
	}
	/* 양수면 오른쪽 모터 정방향 */
	if (speed > 0)
	{
		PORTB |= (1 << PB2);
		PORTB &= ~(1 << PB3);
		OCR1B = (uint8_t)speed;
	}
	/* 음수면 오른쪽 모터 역방향 */
	else if (speed < 0)
	{
		PORTB &= ~(1 << PB2);
		PORTB |= (1 << PB3);
		OCR1B = (uint8_t)(-speed);
	}
	else
	{
		OCR1B = 0;
		PORTB &= ~(1 << PB2);
		PORTB &= ~(1 << PB3);
	}
}

/* 모터 정지 */
void Motor_Stop(void)
{
	Motor_Left(0);
	Motor_Right(0);
}

/* 전진 */
void Motor_Forward(void)
{
	Motor_Left(150);
	Motor_Right(150);
}

/* 후진 */
void Motor_Back(void)
{
	Motor_Left(-150);
	Motor_Right(-150);
}

/* 제자리 좌회전 */
void Motor_Pivot_Left(void)
{
	Motor_Left(-150);
	Motor_Right(150);
}

/* 제자리 우회전 */
void Motor_Pivot_Right(void)
{
	Motor_Left(150);
	Motor_Right(-150);
}

/* IR 값을 0~1000으로 정규화 */
void Normalize_Sensors(void)
{
	uint8_t i;
	uint16_t raw;
	uint16_t min_value;
	uint16_t max_value;
	uint16_t range;
	int32_t value;
	/* 센서 6개를 하나씩 정규화 */
	for (i = 0; i < 6; i++)
	{
		raw = sensor_raw[i];
		min_value = sensor_min[i];
		max_value = sensor_max[i];
		/* 캘리브레이션 값이 잘못되면 0 처리 */
		if (max_value <= min_value)
		{
			sensor_normalized[i] = 0;
			continue;
		}
		range = max_value - min_value;
		/* 센서 변화폭이 너무 작으면 사용 안함 */
		if (range < 40)
		{
			sensor_normalized[i] = 0;
			continue;
		}
		if (raw < min_value)
		{
			raw = min_value;
		}
		if (raw > max_value)
		{
			raw = max_value;
		}
		/* 검은색이 1000에 가깝게 나오도록 변환 */
		value = ((int32_t)(max_value - raw) * 1000) / range;
		if (value < 0)
		{
			value = 0;
		}
		if (value > 1000)
		{
			value = 1000;
		}
		if (value < 40)
		{
			value = 0;
		}
		sensor_normalized[i] = (uint16_t)value;
	}
}

/* 가로선 개수 출력 */
void LCD_Horizon_Print(void)
{
	char buffer[17];
	i2c_lcd_goto_xy(0, 0);
	i2c_lcd_string("                ");
	sprintf(buffer, "HORIZON:%d", HORIZON_COUNT);
	i2c_lcd_goto_xy(0, 0);
	i2c_lcd_string(buffer);
}

/* 흰 배경 맵 캘리브레이션 저장 */
void Save_White_Calibration(void)
{
	uint8_t i;
	for (i = 0; i < 6; i++)
	{
		white_cal_min[i] = sensor_min[i];
		white_cal_max[i] = sensor_max[i];
	}
	white_cal_done = 1;
}

/* 검은 배경 맵 캘리브레이션 저장 */
void Save_Black_Calibration(void)
{
	uint8_t i;
	for (i = 0; i < 6; i++)
	{
		black_cal_min[i] = sensor_min[i];
		black_cal_max[i] = sensor_max[i];
	}
	black_cal_done = 1;
}

/* 흰 배경 맵 캘리브레이션 불러오기 */
void Load_White_Calibration(void)
{
	uint8_t i;
	for (i = 0; i < 6; i++)
	{
		sensor_min[i] = white_cal_min[i];
		sensor_max[i] = white_cal_max[i];
	}
}

/* 검은 배경 맵 캘리브레이션 불러오기 */
void Load_Black_Calibration(void)
{
	uint8_t i;
	for (i = 0; i < 6; i++)
	{
		sensor_min[i] = black_cal_min[i];
		sensor_max[i] = black_cal_max[i];
	}
}

/* IR 센서 캘리브레이션 */
void Calibration(void)
{
	uint8_t i;
	uint16_t count;
	Motor_Stop();
	LED_All_Off();
	/* 최소값 최대값 초기화 */
	for (i = 0; i < 6; i++)
	{
		sensor_min[i] = 1023;
		sensor_max[i] = 0;
	}
	/* 여러 번 읽으면서 센서 최소값 최대값 저장 */
	for (count = 0; count < 1000; count++)
	{
		Read_All_Sensors();
		for (i = 0; i < 6; i++)
		{
			if (sensor_raw[i] < sensor_min[i])
			{
				sensor_min[i] = sensor_raw[i];
			}
			if (sensor_raw[i] > sensor_max[i])
			{
				sensor_max[i] = sensor_raw[i];
			}
		}
		Normalize_Sensors();
		LED_Update();
		_delay_ms(5);
	}
	Read_All_Sensors();
	Normalize_Sensors();
	LED_Update();
	calibration_done = 1;
}

/* 라인 위치 계산 */
uint8_t Calculate_Line_Position(int16_t *position)
{
	uint8_t i;
	uint16_t max_sensor = 0;
	uint32_t total = 0;
	int32_t weighted_sum = 0;
	for (i = 0; i < 6; i++)
	{
		uint16_t value;
		value = Get_Line_Value(i);
		if (value < Get_Line_Threshold(i))
		{
			value = 0;
		}
		if (value > max_sensor)
		{
			max_sensor = value;
		}
		total += value;
		weighted_sum += (int32_t)value * sensor_weight[i];
	}
	if (max_sensor == 0)
	{
		return 0;
	}
	if (total == 0)
	{
		return 0;
	}
	*position = (int16_t)(weighted_sum / (int32_t)total);
	return 1;
}

/* 흰 배경 검은 선 라인트레이싱 */
void Line_Control(void)
{
	int16_t position;
	uint8_t line_found;
	uint8_t sensor_mask;
	uint8_t i;
	Read_All_Sensors();
	Normalize_Sensors();
	LED_Update();
	/* 현재 라인을 보는 센서로 마스크 생성 */
	sensor_mask = 0;
	for (i = 0; i < 6; i++)
	{
		if (Get_Line_Value(i) >= 300)
		{
			sensor_mask |= (1 << i);
		}
	}
	/* ZONE3 대기 중에는 가로선 검사 안함 */
	if (zone3_delay_active)
	{
		cross_checking = 0;
		cross_seen = 0;
		cross_timer = 0;
		left_seen = 0;
		right_seen = 0;
		wide_seen = 0;
		cross_latched = 0;
	}
	else
	{
		if (HORIZON_COUNT == 0)
		{
			if (First_Horizon_Time_Ready())
			{
				HORIZON_COUNTER(sensor_mask);
			}
			else
			{
				cross_checking = 0;
				cross_seen = 0;
				cross_timer = 0;
				left_seen = 0;
				right_seen = 0;
				wide_seen = 0;
				cross_latched = 0;
			}
		}
		else if (HORIZON_COUNT == 1)
		{
			if ((Get_Millis() - run_start_ms) >= 25000UL)
			{
				HORIZON_COUNTER(sensor_mask);
			}
			else
			{
				cross_checking = 0;
				cross_seen = 0;
				cross_timer = 0;
				left_seen = 0;
				right_seen = 0;
				wide_seen = 0;
				cross_latched = 0;
			}
		}
		else
		{
			HORIZON_COUNTER(sensor_mask);
		}
	}
	/* 라인 위치에 따라 좌우 보정 */
	line_found = Calculate_Line_Position(&position);
	if (line_found)
	{
		if (position < -700)
		{
			last_direction = -1;
			Motor_Pivot_Left();
		}
		else if (position > 700)
		{
			last_direction = 1;
			Motor_Pivot_Right();
		}
		else
		{
			if (position < -100)
			{
				last_direction = -1;
			}
			else if (position > 100)
			{
				last_direction = 1;
			}
			Motor_Forward();
		}
	}
	else
	{
		if (last_direction < 0)
		{
			Motor_Pivot_Left();
		}
		else if (last_direction > 0)
		{
			Motor_Pivot_Right();
		}
		else
		{
			Motor_Stop();
		}
	}
}

/* 라인을 보는 IR 개수 확인 */
uint8_t IR_COUNT(uint8_t mask)
{
	uint8_t count = 0;
	uint8_t i;
	for (i = 0; i < 6; i++)
	{
		if (mask & (1 << i))
		{
			count++;
		}
	}
	return count;
}

/* 가로선 인식하고 개수 증가 */
uint8_t HORIZON_COUNTER(uint8_t sensor_mask)
{
	uint8_t count;
	count = IR_COUNT(sensor_mask);
	/* 같은 가로선 두 번 세는 것 방지 */
	if (cross_latched)
	{
		if (count <= 2)
		{
			cross_latched = 0;
		}
		return 0;
	}
	/* 센서 3개 이상이면 가로선 후보 시작 */
	if (!cross_checking)
	{
		if (count >= 3)
		{
			cross_checking = 1;
			cross_seen = 0;
			cross_timer = 0;
			left_seen = 0;
			right_seen = 0;
			wide_seen = 0;
		}
		else
		{
			return 0;
		}
	}
	/* 지나가면서 본 센서를 계속 누적 */
	cross_seen |= sensor_mask;
	if (sensor_mask & (1 << 0))
	{
		left_seen = 1;
	}
	if (sensor_mask & (1 << 5))
	{
		right_seen = 1;
	}
	if (count >= 3)
	{
		wide_seen = 1;
	}
	cross_timer++;
	/* 6개 센서가 전부 누적되면 가로선으로 판단 */
	if (((cross_seen & 0x3F) == 0x3F) && left_seen && right_seen && wide_seen)
	{
		HORIZON_COUNT++;
		LCD_Horizon_Print();
		cross_latched = 1;
		cross_checking = 0;
		cross_seen = 0;
		cross_timer = 0;
		left_seen = 0;
		right_seen = 0;
		wide_seen = 0;
		return 1;
	}
	if (cross_timer >= 60)
	{
		cross_checking = 0;
		cross_seen = 0;
		cross_timer = 0;
		left_seen = 0;
		right_seen = 0;
		wide_seen = 0;
	}
	return 0;
}

/* 첫 가로선 뒤 ZONE2 동작 */
void ZONE_2(void)
{
	int16_t position;
	uint8_t line_found;
	uint32_t turn_start_ms;
	uint32_t ignore_start_ms;
	uint32_t turn_time_ms = 30;
	uint32_t ignore_time_ms = 12000;
	/* ZONE2 들어가서 왼쪽으로 방향 변경 */
	Motor_Stop();
	_delay_ms(100);
	Motor_Pivot_Left();
	_delay_ms(500);
	turn_start_ms = Get_Millis();
	while ((Get_Millis() - turn_start_ms) < turn_time_ms)
	{
		Read_All_Sensors();
		Normalize_Sensors();
		LED_Update();
	}
	cross_seen = 0;
	cross_checking = 0;
	cross_timer = 0;
	left_seen = 0;
	right_seen = 0;
	wide_seen = 0;
	/* 일정 시간 가로선 무시하고 라인트레이싱 */
	ignore_start_ms = Get_Millis();
	while ((Get_Millis() - ignore_start_ms) < ignore_time_ms)
	{
		Read_All_Sensors();
		Normalize_Sensors();
		LED_Update();
		line_found = Calculate_Line_Position(&position);
		if (line_found)
		{
			if (position < -700)
			{
				last_direction = -1;
				Motor_Pivot_Left();
			}
			else if (position > 700)
			{
				last_direction = 1;
				Motor_Pivot_Right();
			}
			else
			{
				if (position < -100)
				{
					last_direction = -1;
				}
				else if (position > 100)
				{
					last_direction = 1;
				}
				Motor_Forward();
			}
		}
		else
		{
			if (last_direction < 0)
			{
				Motor_Pivot_Left();
			}
			else if (last_direction > 0)
			{
				Motor_Pivot_Right();
			}
			else
			{
				Motor_Stop();
			}
		}
		if ((Get_Millis() - last_lcd_update_ms) >= 100)
		{
			last_lcd_update_ms = Get_Millis();
			LCD_Time_Print();
		}
	}
	cross_seen = 0;
	cross_checking = 0;
	cross_timer = 0;
	left_seen = 0;
	right_seen = 0;
	wide_seen = 0;
	cross_latched = 0;
	return;
}

/* 평행사변형 탈출부터 parking까지 진행 */
void ZONE_3(void)
{
	int16_t position;
	uint8_t line_found;
	uint8_t sensor_mask;
	uint8_t i;
	uint32_t zone3_start_ms;
	uint32_t line_trace_start_ms;
	/* ZONE3 시작 시간 저장 */
	zone3_start_ms = Get_Millis();
	/* ZONE3 들어갈 때 방향 잡기 */
	Motor_Stop();
	_delay_ms(1000);
	Motor_Pivot_Right();
	_delay_ms(200);
	Motor_Forward();
	_delay_ms(500);
	while (1)
	{
		Read_All_Sensors();
		Normalize_Sensors();
		LED_Update();
		if ((Get_Millis() - last_lcd_update_ms) >= 100)
		{
			last_lcd_update_ms = Get_Millis();
			LCD_Time_Print();
		}
		sensor_mask = 0;
		for (i = 0; i < 6; i++)
		{
			if (sensor_normalized[i] >= 300)
			{
				sensor_mask |= (1 << i);
			}
		}
		line_found = Calculate_Line_Position(&position);
		/* 벽이 안보이면 그대로 전진 */
		if (!line_found)
		{
			Motor_Forward();
			continue;
		}
		/* 왼쪽 벽이면 오른쪽으로 튕김 */
		if (position < -700)
		{
			last_direction = 1;
			Motor_Back();
			_delay_ms(500);
			Motor_Pivot_Right();
			_delay_ms(250);
			Motor_Forward();
			_delay_ms(250);
			continue;
		}
		/* 오른쪽 벽 처리 */
		else if (position > 700)
		{
			/* 15초 지나고 오른쪽 벽 만나면 ZONE3 탈출 */
			if ((Get_Millis() - zone3_start_ms) >= 15000UL)
			{
				/* 휘면서 평행사변형 탈출 */
				Motor_Left(70);
				Motor_Right(200);
				_delay_ms(1000);
				/* 탈출 후 잠깐 일반 라인트레이싱 */
				line_trace_start_ms = Get_Millis();
				last_direction = 0;
				while ((Get_Millis() - line_trace_start_ms) < 4300UL)
				{
					Read_All_Sensors();
					Normalize_Sensors();
					LED_Update();
					line_found = Calculate_Line_Position( &position );
					if (line_found)
					{
						if (position < -700)
						{
							last_direction = -1;
							Motor_Pivot_Left();
						}
						else if (position > 700)
						{
							last_direction = 1;
							Motor_Pivot_Right();
						}
						else
						{
							if (position < -100)
							{
								last_direction = -1;
							}
							else if (position > 100)
							{
								last_direction = 1;
							}
							Motor_Forward();
						}
					}
					else
					{
						if (last_direction < 0)
						{
							Motor_Pivot_Left();
						}
						else if (last_direction > 0)
						{
							Motor_Pivot_Right();
						}
						else
						{
							Motor_Forward();
						}
					}
					if ((Get_Millis() - last_lcd_update_ms) >= 100)
					{
						last_lcd_update_ms = Get_Millis();
						LCD_Time_Print();
					}
				}
				Motor_Stop();
				_delay_ms(500);
				/* 장애물 없어질 때까지 대기 */
				PSD_Wait_Until_Clear();
				/* PSD 끝나고 3초 라인트레이싱 */
				line_trace_start_ms = Get_Millis();
				last_direction = 0;
				while((Get_Millis() - line_trace_start_ms) < 3000UL)
				{
					Read_All_Sensors();
					Normalize_Sensors();
					LED_Update();
					line_found = Calculate_Line_Position( &position );
					if (line_found)
					{
						if (position < -700)
						{
							last_direction = -1;
							Motor_Pivot_Left();
						}
						else if (position > 700)
						{
							last_direction = 1;
							Motor_Pivot_Right();
						}
						else
						{
							if (position < -100)
							{
								last_direction = -1;
							}
							else if (position > 100)
							{
								last_direction = 1;
							}
							Motor_Forward();
						}
					}
					else
					{
						if (last_direction < 0)
						{
							Motor_Pivot_Left();
						}
						else if (last_direction > 0)
						{
							Motor_Pivot_Right();
						}
						else
						{
							Motor_Forward();
						}
					}
					if ((Get_Millis() - last_lcd_update_ms) >= 100)
					{
						last_lcd_update_ms = Get_Millis();
						if (line_mode == 1)
						{
							LCD_BlackMap_Count_Print();
						}
						else
						{
							LCD_Time_Print();
						}
					}
				}
				Motor_Stop();
				_delay_ms(100);
				/* 마지막으로 parking 실행 */
				parking();
				cross_seen = 0;
				cross_checking = 0;
				cross_timer = 0;
				left_seen = 0;
				right_seen = 0;
				wide_seen = 0;
				cross_latched = 0;
				last_direction = 0;
				return;
			}
			last_direction = -1;
			Motor_Back();
			_delay_ms(500);
			Motor_Pivot_Left();
			_delay_ms(250);
			Motor_Forward();
			_delay_ms(500);
			continue;
		}
		/* 가운데 벽이면 마지막 방향 기준으로 튕김 */
		else
		{
			Motor_Back();
			_delay_ms(500);
			if (last_direction == 1)
			{
				Motor_Back();
				_delay_ms(500);
				Motor_Pivot_Right();
				_delay_ms(300);
				last_direction = -1;
			}
			else
			{
				Motor_Back();
				_delay_ms(500);
				Motor_Pivot_Left();
				_delay_ms(300);
				last_direction = 1;
			}
			Motor_Forward();
			_delay_ms(500);
			continue;
		}
	}
}

/* PSD 값으로 장애물 확인 */
uint8_t PSD_Obstacle(uint16_t psd_value)
{
	if ((psd_value <= 70) && (psd_value >= 100))
	{
		return 1;
	}
	return 0;
}

/* 장애물이 없어질 때까지 대기 */
void PSD_Wait_Until_Clear(void)
{
	uint16_t psd_value;
	uint8_t clear_count = 0;
	char buffer[21];
	i2c_lcd_goto_xy(1, 0);
	i2c_lcd_string( "YES PSD" );
	Motor_Stop();
	/* PSD 값 계속 확인 */
	while (1)
	{
		psd_value = PSD_Read();
		sprintf( buffer, "PSD ADC:%4u    ", psd_value);
		i2c_lcd_goto_xy(0, 0);
		i2c_lcd_string(buffer);
		/* 장애물이 있으면 계속 정지 */
		if (psd_value>= 70)
		{
			Motor_Stop();
			clear_count = 0;
			i2c_lcd_goto_xy(1, 0);
			i2c_lcd_string( "YES OBSTACLE    " );
		}
		/* 장애물 없음이 5번 연속이면 종료 */
		else if (psd_value <= 100)
		{
			Motor_Stop();
			clear_count++;
			i2c_lcd_goto_xy(1, 0);
			i2c_lcd_string( "NO OBSTACLE      ");
			if (clear_count >= 5)
			{
				return;
			}
		}
		_delay_ms(20);
	}
}

/* IR 6개 값 읽기 */
void Read_All_Sensors(void)
{
	uint8_t i;
	for (i = 0; i < 6; i++)
	{
		sensor_raw[i] = ADC_Read(sensor_channel[i]);
	}
}

/* 주차 동작 진행 */
void parking(void)
{
	int16_t position;
	uint8_t line_found;
	uint8_t sensor_mask;
	uint8_t i;
	uint32_t parking_end_time_ms;
	uint8_t first_ir_done = 0;
	/* 이전 가로선 상태 초기화 */
	cross_seen = 0;
	cross_checking = 0;
	cross_timer = 0;
	left_seen = 0;
	right_seen = 0;
	wide_seen = 0;
	cross_latched = 0;
	last_direction = 0;
	while (1)
	{
		Read_All_Sensors();
		Normalize_Sensors();
		LED_Update();
		sensor_mask = 0;
		for (i = 0; i < 6; i++)
		{
			if (sensor_normalized[i] >= 300)
			{
				sensor_mask |= (1 << i);
			}
		}
		/* 1단계 IR1 찾기 */
		if (first_ir_done == 0)
		{
			if ((sensor_mask & 0b000001) == 0b000001)
			{
				i2c_lcd_goto_xy(1, 0);
				i2c_lcd_string("                ");
				i2c_lcd_goto_xy(1, 0);
				i2c_lcd_string("PARKING");
				Motor_Stop();
				_delay_ms(1000);
				Motor_Forward();
				_delay_ms(1100);
				Motor_Stop();
				_delay_ms(1000);
				Motor_Pivot_Left();
				_delay_ms(1200);
				Motor_Stop();
				_delay_ms(1000);
				first_ir_done = 1;
				cross_seen = 0;
				cross_checking = 0;
				cross_timer = 0;
				left_seen = 0;
				right_seen = 0;
				wide_seen = 0;
				cross_latched = 0;
				last_direction = 0;
				continue;
			}
		}
		/* 2단계 가로선 찾기 */
		if (first_ir_done == 1)
		{
			/* 가로선 찾으면 parking 마무리 동작 */
			if (HORIZON_COUNTER(sensor_mask))
			{
				i2c_lcd_goto_xy(1, 0);
				i2c_lcd_string("                ");
				i2c_lcd_goto_xy(1, 0);
				i2c_lcd_string("END PARKING");
				Motor_Stop();
				_delay_ms(1000);
				Motor_Forward();
				_delay_ms(1000);
				Motor_Stop();
				_delay_ms(1000);
				Motor_Pivot_Left();
				_delay_ms(3000);
				Motor_Stop();
				_delay_ms(1000);
				cross_seen = 0;
				cross_checking = 0;
				cross_timer = 0;
				left_seen = 0;
				right_seen = 0;
				wide_seen = 0;
				cross_latched = 0;
				last_direction = 0;
				/* parking 끝나고 3초 라인트레이싱 */
				parking_end_time_ms = Get_Millis();
				while ((Get_Millis() - parking_end_time_ms) < 3000UL)
				{
					Read_All_Sensors();
					Normalize_Sensors();
					LED_Update();
					line_found = Calculate_Line_Position(&position);
					if (line_found)
					{
						if (position < -700)
						{
							last_direction = -1;
							Motor_Pivot_Left();
						}
						else if (position > 700)
						{
							last_direction = 1;
							Motor_Pivot_Right();
						}
						else
						{
							if (position < -100)
							{
								last_direction = -1;
							}
							else if (position > 100)
							{
								last_direction = 1;
							}
							Motor_Forward();
						}
					}
					else
					{
						if (last_direction < 0)
						{
							Motor_Pivot_Left();
						}
						else if (last_direction > 0)
						{
							Motor_Pivot_Right();
						}
						else
						{
							Motor_Forward();
						}
					}
					if ((Get_Millis() - last_lcd_update_ms) >= 100UL)
					{
						last_lcd_update_ms = Get_Millis();
						LCD_Time_Print();
					}
				}
				Motor_Stop();
				_delay_ms(100);
				Black_zone = 1;
				last_direction = 0;
				return;
			}
		}
		line_found = Calculate_Line_Position(&position);
		if (line_found)
		{
			if (position < -700)
			{
				last_direction = -1;
				Motor_Pivot_Left();
			}
			else if (position > 700)
			{
				last_direction = 1;
				Motor_Pivot_Right();
			}
			else
			{
				if (position < -100)
				{
					last_direction = -1;
				}
				else if (position > 100)
				{
					last_direction = 1;
				}
				Motor_Forward();
			}
		}
		else
		{
			if (last_direction < 0)
			{
				Motor_Pivot_Left();
			}
			else if (last_direction > 0)
			{
				Motor_Pivot_Right();
			}
			else
			{
				Motor_Forward();
			}
		}
		if ((Get_Millis() - last_lcd_update_ms) >= 100UL)
		{
			last_lcd_update_ms = Get_Millis();
			LCD_Time_Print();
		}
	}
}

/* 현재 맵에 맞는 라인값 사용 */
uint16_t Get_Line_Value(uint8_t index)
{
	uint16_t range;
	if (sensor_max[index] <= sensor_min[index]) return 0;
	range = sensor_max[index] - sensor_min[index];
	if (range < 40)
	return 0;
	if (line_mode == 0)
	{
		return sensor_normalized[index];
	}
	else
	{
		return 1000 - sensor_normalized[index];
	}
}

/* 검은 배경 맵 캘리브레이션 */
void blackmap_calibration(void)
{
	uint8_t i;
	for (i = 0; i < 6; i++)
	{
		sensor_min[i] = 1023;
		sensor_max[i] = 0;
	}
	/* 왼쪽으로 돌면서 센서 범위 측정 */
	bm_calibration_start_ms = Get_Millis();
	Motor_Pivot_Left();
	while ((Get_Millis() - bm_calibration_start_ms) < 600UL)
	{
		Read_All_Sensors();
		for (i = 0; i < 6; i++)
		{
			if (sensor_raw[i] < sensor_min[i])
			{
				sensor_min[i] = sensor_raw[i];
			}
			if (sensor_raw[i] > sensor_max[i])
			{
				sensor_max[i] = sensor_raw[i];
			}
		}
	}
	/* 오른쪽으로 돌면서 센서 범위 측정 */
	bm_calibration_start_ms = Get_Millis();
	Motor_Pivot_Right();
	while ((Get_Millis() - bm_calibration_start_ms) < 1200UL)
	{
		Read_All_Sensors();
		for (i = 0; i < 6; i++)
		{
			if (sensor_raw[i] < sensor_min[i])
			{
				sensor_min[i] = sensor_raw[i];
			}
			if (sensor_raw[i] > sensor_max[i])
			{
				sensor_max[i] = sensor_raw[i];
			}
		}
	}
	bm_calibration_start_ms = Get_Millis();
	Motor_Pivot_Left();
	while ((Get_Millis() - bm_calibration_start_ms) < 600UL)
	{
		Read_All_Sensors();
		for (i = 0; i < 6; i++)
		{
			if (sensor_raw[i] < sensor_min[i])
			{
				sensor_min[i] = sensor_raw[i];
			}
			if (sensor_raw[i] > sensor_max[i])
			{
				sensor_max[i] = sensor_raw[i];
			}
		}
	}
	Motor_Stop();
	_delay_ms(300);
	Read_All_Sensors();
	for (i = 0; i < 6; i++)
	{
		if (sensor_raw[i] < sensor_min[i])
		{
			sensor_min[i] = sensor_raw[i];
		}
		if (sensor_raw[i] > sensor_max[i])
		{
			sensor_max[i] = sensor_raw[i];
		}
	}
	Read_All_Sensors();
	Normalize_Sensors();
}

/* 라인 찾으면서 좌회전 */
void Turn_90_Left(void)
{
	uint32_t turn_start_ms;
	uint8_t center_count = 0;
	turn_start_ms = Get_Millis();
	Motor_Pivot_Left();
	while (1)
	{
		Read_All_Sensors();
		Normalize_Sensors();
		LED_Update();
		if ((Get_Millis() - turn_start_ms) < 800UL)
		continue;
		/* 가운데 센서가 라인을 연속으로 찾으면 회전 종료 */
		/* 가운데 센서가 라인을 연속으로 찾으면 회전 종료 */
		/* 가운데 센서가 라인을 연속으로 찾으면 회전 종료 */
		if ((Get_Line_Value(2) >= Get_Line_Threshold(2)) || (Get_Line_Value(3) >= Get_Line_Threshold(3)))
		{
			center_count++;
			if (center_count >= 5)
			{
				Motor_Stop();
				last_direction = 0;
				return;
			}
		}
		else
		{
			center_count = 0;
		}
		if ((Get_Millis() - turn_start_ms) >= 3000UL)
		{
			Motor_Stop();
			last_direction = 0;
			return;
		}
	}
}

/* 라인 찾으면서 크게 우회전 */
void Turn_120_Right(void)
{
	uint32_t turn_start_ms;
	uint8_t center_count = 0;
	turn_start_ms = Get_Millis();
	Motor_Pivot_Right();
	while (1)
	{
		Read_All_Sensors();
		Normalize_Sensors();
		LED_Update();
		if ((Get_Millis() - turn_start_ms) < 1500UL)
		continue;
		if ((Get_Line_Value(2) >= Get_Line_Threshold(2)) || (Get_Line_Value(3) >= Get_Line_Threshold(3)))
		{
			center_count++;
			if (center_count >= 5)
			{
				Motor_Stop();
				last_direction = 0;
				return;
			}
		}
		else
		{
			center_count = 0;
		}
		if ((Get_Millis() - turn_start_ms) >= 3000UL)
		{
			Motor_Stop();
			last_direction = 0;
			return;
		}
	}
}

/* 라인 찾으면서 우회전 */
void Turn_90_Right(void)
{
	uint32_t turn_start_ms;
	uint8_t center_count = 0;
	turn_start_ms = Get_Millis();
	Motor_Pivot_Right();
	while (1)
	{
		Read_All_Sensors();
		Normalize_Sensors();
		LED_Update();
		if ((Get_Millis() - turn_start_ms) < 800UL)
		continue;
		if ((Get_Line_Value(2) >= Get_Line_Threshold(2)) || (Get_Line_Value(3) >= Get_Line_Threshold(3)))
		{
			center_count++;
			if (center_count >= 5)
			{
				Motor_Stop();
				last_direction = 0;
				return;
			}
		}
		else
		{
			center_count = 0;
		}
		if ((Get_Millis() - turn_start_ms) >= 3000UL)
		{
			Motor_Stop();
			last_direction = 0;
			return;
		}
	}
}

/* 현재 맵의 라인 임계값 사용 */
uint16_t Get_Line_Threshold(uint8_t index)
{
	if (line_mode == 0)
	{
		return 300;
	}
	return white_map_threshold[index];
}

/* 검은 배경 흰 선 구간 주행 */
void Black_Map_Line_Control(void)
{
	int16_t position;
	uint8_t line_found;
	uint8_t sensor_mask = 0;
	uint8_t i;
	uint8_t count;
	static uint8_t detect_state = 0;
	static uint8_t candidate_direction = 0;
	static uint8_t accumulated_mask = 0;
	static uint32_t detect_start_ms = 0;
	static uint32_t overlap_start_ms = 0;
	static uint8_t align_center_count = 0;
	Read_All_Sensors();
	Normalize_Sensors();
	LED_Update();
	/* 검은 맵 처음 들어오면 라인 중앙 정렬 */
	if (blackmap_align_done == 0)
	{
		detect_state = 0;
		candidate_direction = 0;
		accumulated_mask = 0;
		line_found = Calculate_Line_Position(&position);
		if (line_found)
		{
			if (position < -700)
			{
				last_direction = -1;
				align_center_count = 0;
				Motor_Pivot_Left();
			}
			else if (position > 700)
			{
				last_direction = 1;
				align_center_count = 0;
				Motor_Pivot_Right();
			}
			else
			{
				Motor_Right(150);
				Motor_Left(150);
				if ((position >= -200) && (position <= 200))
				{
					align_center_count++;
					if (align_center_count >= 5)
					{
						blackmap_align_done = 1;
						align_center_count = 0;
						last_direction = 0;
						Motor_Forward();
						return;
					}
				}
				else
				{
					align_center_count = 0;
				}
			}
		}
		else
		{
			align_center_count = 0;
			if (last_direction < 0)
			{
				Motor_Pivot_Left();
			}
			else if (last_direction > 0)
			{
				Motor_Pivot_Right();
			}
			else
			{
				Motor_Right(150);
				Motor_Left(150);
			}
		}
		return;
	}
	for (i = 0; i < 6; i++)
	{
		if (Get_Line_Value(i) >= Get_Line_Threshold(i))
		{
			sensor_mask |= (1 << i);
		}
	}
	count = IR_COUNT(sensor_mask);
	/* 넓은 교차구간 빠져나가는 중 */
	if (detect_state == 2)
	{
		Motor_Left(150);
		Motor_Right(150);
		if ((Get_Millis() - overlap_start_ms) < 300UL)
		{
			return;
		}
		if ((count <= 3) && ((sensor_mask & 0b001100) != 0))
		{
			detect_state = 0;
			candidate_direction = 0;
			accumulated_mask = 0;
			last_direction = 0;
		}
		return;
	}
	/* 4개 이상 인식 후 좌우 방향 판단 */
	if (detect_state == 1)
	{
		accumulated_mask |= sensor_mask;
		/* 6개가 전부 누적되면 교차구간으로 판단 */
		if ((accumulated_mask & 0x3F) == 0x3F)
		{
			detect_state = 2;
			candidate_direction = 0;
			accumulated_mask = 0;
			overlap_start_ms = Get_Millis();
			Motor_Left(150);
			Motor_Right(150);
			return;
		}
		if ((sensor_mask & 0b001111) == 0b001111)
		{
			candidate_direction = 1;
		}
		if ((sensor_mask & 0b111100) == 0b111100)
		{
			candidate_direction = 2;
		}
		if ((Get_Millis() - detect_start_ms) < 500UL)
		{
			Motor_Left(150);
			Motor_Right(150);
			return;
		}
		detect_state = 0;
		accumulated_mask = 0;
		/* 왼쪽 방향이면 좌회전 */
		if (candidate_direction == 1)
		{
			candidate_direction = 0;
			blackmap_turn_left_count++;
			Motor_Stop();
			_delay_ms(500);
			Motor_Forward();
			_delay_ms(800);
			if (blackmap_turn_left_count == 3)
			{
				Turn_90_Left();
				Motor_Stop();
				_delay_ms(2000);
				PSD_Forward_Until_Target();
				blackmap_normal_mode = 1;
				Motor_Stop();
				_delay_ms(1000);
				last_direction = 0;
				return;
			}
			else
			{
				Turn_90_Left();
			}
			Motor_Forward();
			_delay_ms(100);
			last_direction = 0;
			return;
		}
		/* 오른쪽 방향이면 우회전 */
		if (candidate_direction == 2)
		{
			candidate_direction = 0;
			blackmap_turn_right_count++;
			Motor_Stop();
			_delay_ms(500);
			Motor_Forward();
			_delay_ms(800);
			if (blackmap_turn_right_count == 5)
			{
				Motor_Forward();
				_delay_ms(500);
				Motor_Pivot_Right();
				_delay_ms(1700);
				Motor_Forward();
				_delay_ms(1500);
			}
			else
			{
				Turn_90_Right();
			}
			Motor_Forward();
			_delay_ms(100);
			last_direction = 0;
			return;
		}
		candidate_direction = 0;
		return;
	}
	/* 센서 4개 이상이면 교차구간 후보 시작 */
	if (count >= 4)
	{
		detect_state = 1;
		accumulated_mask = sensor_mask;
		detect_start_ms = Get_Millis();
		candidate_direction = 0;
		if ((sensor_mask & 0b001111) == 0b001111)
		{
			candidate_direction = 1;
		}
		if ((sensor_mask & 0b111100) == 0b111100)
		{
			candidate_direction = 2;
		}
		Motor_Left(150);
		Motor_Right(150);
		return;
	}
	line_found = Calculate_Line_Position(&position);
	if (line_found)
	{
		if (position < -700)
		{
			last_direction = -1;
			Motor_Pivot_Left();
		}
		else if (position > 700)
		{
			last_direction = 1;
			Motor_Pivot_Right();
		}
		else
		{
			if (position < -100)
			{
				last_direction = -1;
			}
			else if (position > 100)
			{
				last_direction = 1;
			}
			Motor_Left(150);
			Motor_Right(150);
		}
	}
	/* 라인 놓치면 좌우로 돌면서 다시 찾기 */
	else
	{
		Turn_90_Left();
		Motor_Stop();
		_delay_ms(500);
		Turn_90_Right();
		Motor_Stop();
		_delay_ms(500);
		Turn_90_Right();
		Motor_Stop();
		_delay_ms(500);
		last_direction = 0;
		return;
	}
}

/* PSD 목표값까지 전진 */
void PSD_Forward_Until_Target(void)
{
	uint16_t psd_value;
	char buffer[21];
	while (1)
	{
		psd_value = PSD_Read();
		sprintf(buffer,"PSD ADC:%4u    ", psd_value);
		i2c_lcd_goto_xy(0, 0);
		i2c_lcd_string(buffer);
		/* 목표 거리 도착하면 다음 동작 실행 */
		if (psd_value > 110)
		{
			Motor_Stop();
			_delay_ms(2000);
			Motor_Forward();
			_delay_ms(500);
			Motor_Pivot_Left();
			_delay_ms(1000);
			Motor_Stop();
			_delay_ms(1000);
			Motor_Forward();
			_delay_ms(2000);
			return;
		}
		else
		{
			Motor_Forward();
		}
		_delay_ms(20);
	}
}

/* 검은 맵 시간과 좌우회전 횟수 출력 */
void LCD_BlackMap_Count_Print(void)
{
	char buffer[17];
	uint32_t elapsed_ms;
	uint32_t sec;
	uint16_t decimal;
	elapsed_ms = Get_Millis() - run_start_ms;
	sec = elapsed_ms / 1000;
	decimal = (elapsed_ms % 1000) / 100;
	i2c_lcd_goto_xy(0, 0);
	sprintf(buffer, "TIME:%lu.%u sec ", (unsigned long)sec, (unsigned int)decimal);
	i2c_lcd_string(buffer);
	i2c_lcd_goto_xy(1, 0);
	sprintf(buffer, "L:%d R:%d        ", blackmap_turn_left_count, blackmap_turn_right_count);
	i2c_lcd_string(buffer);
}

/* 검은 맵 일반 라인트레이싱 */
void Black_Map_Normal_Line_Control(void)
{
	int16_t position;
	uint8_t line_found;
	uint8_t sensor_mask = 0;
	uint8_t i;
	Read_All_Sensors();
	Normalize_Sensors();
	LED_Update();
	for (i = 0; i < 6; i++)
	{
		if (Get_Line_Value(i) >= Get_Line_Threshold(i))
		{
			sensor_mask |= (1 << i);
		}
	}
	/* 검은 맵에서도 가로선 계속 확인 */
	HORIZON_COUNTER(sensor_mask);
	/* 라인 위치에 맞춰 일반 주행 */
	line_found = Calculate_Line_Position(&position);
	if (line_found)
	{
		if (position < -700)
		{
			last_direction = -1;
			Motor_Pivot_Left();
		}
		else if (position > 700)
		{
			last_direction = 1;
			Motor_Pivot_Right();
		}
		else
		{
			if (position < -100)
			{
				last_direction = -1;
			}
			else if (position > 100)
			{
				last_direction = 1;
			}
			else
			{
				last_direction = 0;
			}
			Motor_Forward();
		}
	}
	else
	{
		if (last_direction < 0)
		{
			Motor_Pivot_Left();
		}
		else if (last_direction > 0)
		{
			Motor_Pivot_Right();
		}
		else
		{
			Motor_Forward();
		}
	}
}

/* 전체 주행 순서 관리 */
int main(void)
{
	uint8_t zone2_done = 0;
	uint8_t zone3_done = 0;
	/* 장치 초기화 */
	JTAG_Disable();
	ADC_Init();
	Motor_Init();
	PWM_Init();
	Switch_Init();
	LED_Init();
	i2c_lcd_init();
	i2c_lcd_clear();
	Timer0_Init();
	sei();
	LCD_Horizon_Print();
	Motor_Stop();
	LED_All_Off();
	while (1)
	{
		/* 주행 전 캘리브레이션과 시작 버튼 처리 */
		if (!running)
		{
			/* SW1 첫 번째는 흰 배경 맵 캘리브레이션 */
			if (Switch_Pressed(PE4))
			{
				if (white_cal_done == 0)
				{
					line_mode = 0;
					i2c_lcd_clear();
					i2c_lcd_goto_xy(0, 0);
					i2c_lcd_string("WHITE MAP CAL");
					Calibration();
					Save_White_Calibration();
					i2c_lcd_goto_xy(1, 0);
					i2c_lcd_string("WHITE CAL END");
				}
				/* SW1 두 번째는 검은 배경 맵 캘리브레이션 */
				else if (black_cal_done == 0)
				{
					line_mode = 1;
					i2c_lcd_clear();
					i2c_lcd_goto_xy(0, 0);
					i2c_lcd_string("BLACK MAP CAL");
					blackmap_calibration();
					Save_Black_Calibration();
					Load_White_Calibration();
					line_mode = 0;
					calibration_done = 1;
					i2c_lcd_goto_xy(1, 0);
					i2c_lcd_string("ALL CAL END");
				}
			}
			/* 두 캘리브레이션이 끝나면 SW2로 주행 시작 */
			if (white_cal_done && black_cal_done)
			{
				if (Switch_Pressed(PE5))
				{
					running = 1;
					Load_White_Calibration();
					line_mode = 0;
					last_direction = 0;
					HORIZON_COUNT = 0;
					zone2_done = 0;
					zone3_done = 0;
					zone3_delay_active = 0;
					zone3_delay_start_ms = 0;
					zone_change_done = 0;
					cross_seen = 0;
					cross_checking = 0;
					cross_timer = 0;
					left_seen = 0;
					right_seen = 0;
					wide_seen = 0;
					cross_latched = 0;
					run_start_ms = Get_Millis();
					last_lcd_update_ms = run_start_ms;
					LCD_Time_Print();
				}
			}
			if (calibration_done)
			{
				if (Switch_Pressed(PE5))
				{
					running = 1;
					last_direction = 0;
					HORIZON_COUNT = 0;
					zone2_done = 0;
					zone3_done = 0;
					zone3_delay_active = 0;
					zone3_delay_start_ms = 0;
					line_mode = 0;
					zone_change_done = 0;
					cross_seen = 0;
					cross_checking = 0;
					cross_timer = 0;
					left_seen = 0;
					right_seen = 0;
					wide_seen = 0;
					cross_latched = 0;
					run_start_ms =  Get_Millis();
					last_lcd_update_ms = run_start_ms;
					LCD_Time_Print();
				}
			}
			if (!running)
			{
				Motor_Stop();
			}
		}
		/* 주행 시작 */
		else
		{
			/* 흰 배경 검은 선 주행 */
			if (line_mode == 0)
			{
				Line_Control();
			}
			/* 검은 배경 흰 선 주행 */
			else
			{
				if (blackmap_normal_mode == 0)
				{
					Black_Map_Line_Control();
				}
				else
				{
					Black_Map_Normal_Line_Control();
				}
			}
			if ((Get_Millis() - last_lcd_update_ms) >= 100)
			{
				last_lcd_update_ms = Get_Millis();
				LCD_Time_Print();
			}
			/* 첫 가로선 뒤 ZONE2 실행 */
			if ((HORIZON_COUNT == 1) && (zone2_done == 0))
			{
				zone2_done = 1;
				i2c_lcd_goto_xy(1, 0);
				i2c_lcd_string("                ");
				i2c_lcd_goto_xy(1, 0);
				i2c_lcd_string( "ZONE2" );
				ZONE_2();
			}
			/* 두 번째 가로선 뒤 ZONE3 준비 */
			if ((HORIZON_COUNT == 2) && (zone3_done == 0))
			{
				if (zone3_delay_active == 0)
				{
					zone3_delay_active = 1;
					zone3_delay_start_ms = Get_Millis();
					cross_seen = 0;
					cross_checking = 0;
					cross_timer = 0;
					left_seen = 0;
					right_seen = 0;
					wide_seen = 0;
					cross_latched = 0;
				}
				if (zone3_delay_active && ((Get_Millis() - zone3_delay_start_ms) >= 2500UL))
				{
					zone3_delay_active = 0;
					zone3_done = 1;
					cross_seen = 0;
					cross_checking = 0;
					cross_timer = 0;
					left_seen = 0;
					right_seen = 0;
					wide_seen = 0;
					cross_latched = 0;
					i2c_lcd_goto_xy(1, 0);
					i2c_lcd_string("                ");
					i2c_lcd_goto_xy(1, 0);
					i2c_lcd_string("ZONE3");
					ZONE_3();
				}
			}
			/* parking 끝나면 검은 배경 흰 선 모드로 변경 */
			if ((Black_zone == 1) && (zone_change_done == 0)){
				zone_change_done = 1;
				i2c_lcd_goto_xy(1, 0);
				i2c_lcd_string("                ");
				i2c_lcd_goto_xy(1, 0);
				i2c_lcd_string( "ZONE_change" );
				Motor_Stop();
				_delay_ms(2000);
				Motor_Forward();
				_delay_ms(1000);
				Motor_Stop();
				_delay_ms(100);
				Load_Black_Calibration();
				line_mode = 1;
				blackmap_turn_right_count = 0;
				blackmap_turn_left_count = 0;
				blackmap_align_done = 0;
				last_direction = 0;
				cross_seen = 0;
				cross_checking = 0;
				cross_timer = 0;
				left_seen = 0;
				right_seen = 0;
				wide_seen = 0;
				cross_latched = 0;
			}
		}
	}
	return 0;
}

/* 1ms마다 전체 시간 1 증가 */
ISR(TIMER0_COMP_vect)
{
	system_ms++;
}
