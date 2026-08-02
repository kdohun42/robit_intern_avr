#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdint.h>
#include "i2c_lcd.h"

#define SW1_CONFIRM PE4
#define SW2_START   PE5

#define ADC_SAMPLE_COUNT 8

typedef enum
{
	SET_YEAR,
	SET_MONTH,
	SET_DAY,
	SET_HOUR,
	SET_MINUTE,
	SET_SECOND,
	SET_MILLISECOND,
	SET_COMPLETE
} SettingField;

typedef struct
{
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint16_t millisecond;
} DateTime;

volatile DateTime clock_time =
{
	0,
	1,
	1,
	0,
	0,
	0,
	0
};

void Switch_Init(void)
{
	// PE4와 PE5를 입력으로 설정
	DDRE &= ~((1 << SW1_CONFIRM) | (1 << SW2_START));

	// PE4와 PE5 내부 풀업 저항 활성화
	PORTE |= (1 << SW1_CONFIRM) | (1 << SW2_START);
}

uint8_t Switch_Pressed(uint8_t pin)
{
	// 스위치가 눌렸는지 확인
	if (!(PINE & (1 << pin)))
	{
		// 스위치 채터링 방지
		_delay_ms(20);

		// 스위치 상태 다시 확인
		if (!(PINE & (1 << pin)))
		{
			// 스위치를 뗄 때까지 대기
			while (!(PINE & (1 << pin)));

			// 스위치를 뗄 때 채터링 방지
			_delay_ms(20);

			return 1;
		}
	}

	return 0;
}

void ADC_Init(void)
{
	// PF0을 입력으로 설정
	DDRF &= ~(1 << DDF0);

	// PF0 내부 풀업 저항 비활성화
	PORTF &= ~(1 << PF0);

	// 기준전압 AVCC와 ADC0 선택
	ADMUX = (1 << REFS0);

	// ADC 활성화와 분주비 128 설정
	ADCSRA =
	(1 << ADEN) |
	(1 << ADPS2) |
	(1 << ADPS1) |
	(1 << ADPS0);

	// ADC 안정화 대기
	_delay_ms(1);

	// 첫 번째 ADC 변환 시작
	ADCSRA |= (1 << ADSC);

	// ADC 변환 완료 대기
	while (ADCSRA & (1 << ADSC));

	// 첫 번째 ADC 결과 버리기
	(void)ADC;
}

uint16_t ADC_Read(void)
{
	// ADC 변환 시작
	ADCSRA |= (1 << ADSC);

	// ADC 변환 완료 대기
	while (ADCSRA & (1 << ADSC));

	// ADC 결과 반환
	return ADC;
}

uint16_t ADC_ReadAverage(void)
{
	uint32_t sum = 0;
	uint8_t i;

	// ADC 값을 여러 번 측정
	for (i = 0; i < ADC_SAMPLE_COUNT; i++)
	{
		sum += ADC_Read();
	}

	// ADC 평균값 반환
	return (uint16_t)(sum / ADC_SAMPLE_COUNT);
}

uint16_t ADC_Map(
uint16_t adc_value,
uint16_t minimum,
uint16_t maximum)
{
	uint32_t result;

	// ADC 값을 지정한 범위로 변환
	result =
	(uint32_t)adc_value *
	(maximum - minimum);

	// 소수점 반올림 후 범위값 계산
	result = (result + 511UL) / 1023UL;

	return minimum + (uint16_t)result;
}

uint8_t Is_LeapYear(uint8_t year)
{
	// 2000년부터 2099년 사이의 윤년 확인
	if ((year % 4) == 0)
	{
		return 1;
	}

	return 0;
}

uint8_t Days_InMonth(uint8_t year, uint8_t month)
{
	// 2월의 마지막 날짜 계산
	if (month == 2)
	{
		if (Is_LeapYear(year))
		{
			return 29;
		}

		return 28;
	}

	// 30일까지 있는 월 확인
	if (month == 4 ||
	month == 6 ||
	month == 9 ||
	month == 11)
	{
		return 30;
	}

	// 나머지 월은 31일까지 사용
	return 31;
}

void LCD_WriteLine(uint8_t row, const char *text)
{
	char line[17];
	uint8_t i;

	// LCD 한 줄을 공백으로 초기화
	for (i = 0; i < 16; i++)
	{
		line[i] = ' ';
	}

	line[16] = '\0';

	// 문자열을 LCD 출력 배열에 복사
	i = 0;

	while (text[i] != '\0' && i < 16)
	{
		line[i] = text[i];
		i++;
	}

	// 지정한 LCD 줄에 문자열 출력
	i2c_lcd_goto_xy(row, 0);
	i2c_lcd_string(line);
}

void Get_ClockSnapshot(DateTime *copy)
{
	uint8_t saved_sreg;

	// 현재 상태 레지스터 저장
	saved_sreg = SREG;

	// 시간 복사 중 인터럽트 비활성화
	cli();

	copy->year = clock_time.year;
	copy->month = clock_time.month;
	copy->day = clock_time.day;
	copy->hour = clock_time.hour;
	copy->minute = clock_time.minute;
	copy->second = clock_time.second;
	copy->millisecond = clock_time.millisecond;

	// 기존 인터럽트 상태 복원
	SREG = saved_sreg;
}

void Display_DateTime(const DateTime *time)
{
	char line1[17];
	char line2[17];

	// LCD 첫 번째 줄 날짜 문자열 생성
	snprintf(
	line1,
	sizeof(line1),
	"%02u-%02u-%02u",
	(unsigned int)time->year,
	(unsigned int)time->month,
	(unsigned int)time->day
	);

	// LCD 두 번째 줄 시간 문자열 생성
	snprintf(
	line2,
	sizeof(line2),
	"%02u:%02u:%02u.%03u",
	(unsigned int)time->hour,
	(unsigned int)time->minute,
	(unsigned int)time->second,
	(unsigned int)time->millisecond
	);

	// LCD에 날짜 출력
	LCD_WriteLine(0, line1);

	// LCD에 시간 출력
	LCD_WriteLine(1, line2);
}

void Update_SettingValue(
SettingField field,
uint16_t adc_value)
{
	uint8_t maximum_day;

	// 연도를 00부터 99까지 설정
	if (field == SET_YEAR)
	{
		clock_time.year =
		(uint8_t)ADC_Map(adc_value, 0, 99);
	}

	// 월을 1부터 12까지 설정
	else if (field == SET_MONTH)
	{
		clock_time.month =
		(uint8_t)ADC_Map(adc_value, 1, 12);
	}

	// 일을 현재 연도와 월에 맞게 설정
	else if (field == SET_DAY)
	{
		maximum_day =
		Days_InMonth(
		clock_time.year,
		clock_time.month
		);

		clock_time.day =
		(uint8_t)ADC_Map(
		adc_value,
		1,
		maximum_day
		);
	}

	// 시간을 0부터 23까지 설정
	else if (field == SET_HOUR)
	{
		clock_time.hour =
		(uint8_t)ADC_Map(adc_value, 0, 23);
	}

	// 분을 0부터 59까지 설정
	else if (field == SET_MINUTE)
	{
		clock_time.minute =
		(uint8_t)ADC_Map(adc_value, 0, 59);
	}

	// 초를 0부터 59까지 설정
	else if (field == SET_SECOND)
	{
		clock_time.second =
		(uint8_t)ADC_Map(adc_value, 0, 59);
	}

	// 밀리초를 0부터 999까지 설정
	else if (field == SET_MILLISECOND)
	{
		clock_time.millisecond =
		ADC_Map(adc_value, 0, 999);
	}
}

uint8_t DateTime_IsValid(const DateTime *time)
{
	uint8_t maximum_day;

	// 연도 범위 확인
	if (time->year > 99)
	{
		return 0;
	}

	// 월 범위 확인
	if (time->month < 1 || time->month > 12)
	{
		return 0;
	}

	// 현재 월의 마지막 날짜 계산
	maximum_day =
	Days_InMonth(time->year, time->month);

	// 일 범위 확인
	if (time->day < 1 || time->day > maximum_day)
	{
		return 0;
	}

	// 시간 범위 확인
	if (time->hour > 23)
	{
		return 0;
	}

	// 분 범위 확인
	if (time->minute > 59)
	{
		return 0;
	}

	// 초 범위 확인
	if (time->second > 59)
	{
		return 0;
	}

	// 밀리초 범위 확인
	if (time->millisecond > 999)
	{
		return 0;
	}

	return 1;
}

void DateTime_Reset(void)
{
	// 날짜와 시간을 기본값으로 초기화
	clock_time.year = 0;
	clock_time.month = 1;
	clock_time.day = 1;
	clock_time.hour = 0;
	clock_time.minute = 0;
	clock_time.second = 0;
	clock_time.millisecond = 0;
}

void Timer0_Init(void)
{
	// Timer0 정지
	TCCR0 = 0x00;

	// Timer0 카운터 초기화
	TCNT0 = 0;

	// 1밀리초 비교 일치값 설정
	OCR0 = 249;

	// Timer0 CTC 모드 설정
	TCCR0 = (1 << WGM01);

	// Timer0 비교 일치 인터럽트 활성화
	TIMSK |= (1 << OCIE0);
}

void Timer0_Start(void)
{
	// Timer0 카운터 초기화
	TCNT0 = 0;

	// 기존 비교 일치 플래그 제거
	TIFR = (1 << OCF0);

	// CTC 모드와 분주비 64 설정
	TCCR0 =
	(1 << WGM01) |
	(1 << CS01) |
	(1 << CS00);

	// 전역 인터럽트 활성화
	sei();
}

ISR(TIMER0_COMP_vect)
{
	// 밀리초 증가
	clock_time.millisecond++;

	// 밀리초가 1000이 되면 초 증가
	if (clock_time.millisecond >= 1000)
	{
		clock_time.millisecond = 0;
		clock_time.second++;

		// 초가 60이 되면 분 증가
		if (clock_time.second >= 60)
		{
			clock_time.second = 0;
			clock_time.minute++;

			// 분이 60이 되면 시간 증가
			if (clock_time.minute >= 60)
			{
				clock_time.minute = 0;
				clock_time.hour++;

				// 시간이 24가 되면 날짜 증가
				if (clock_time.hour >= 24)
				{
					clock_time.hour = 0;
					clock_time.day++;

					// 현재 월의 마지막 날짜를 넘으면 월 증가
					if (clock_time.day >
					Days_InMonth(
					clock_time.year,
					clock_time.month
					))
					{
						clock_time.day = 1;
						clock_time.month++;

						// 월이 12를 넘으면 연도 증가
						if (clock_time.month > 12)
						{
							clock_time.month = 1;
							clock_time.year++;

							// 연도가 99를 넘으면 00으로 변경
							if (clock_time.year > 99)
							{
								clock_time.year = 0;
							}
						}
					}
				}
			}
		}
	}
}

int main(void)
{
	SettingField field = SET_YEAR;
	uint16_t adc_value;
	DateTime current_time;

	// I2C LCD 초기화
	i2c_lcd_init();

	// 스위치 초기화
	Switch_Init();

	// ADC 초기화
	ADC_Init();

	// Timer0 초기화
	Timer0_Init();

	// LCD 화면 지우기
	i2c_lcd_clear();

	// 초기 날짜와 시간 출력
	Get_ClockSnapshot(&current_time);
	Display_DateTime(&current_time);

	// 연도부터 밀리초까지 순서대로 설정
	while (field < SET_COMPLETE)
	{
		// 가변저항 ADC 값 읽기
		adc_value = ADC_ReadAverage();

		// 현재 설정 항목 값 변경
		Update_SettingValue(field, adc_value);

		// 변경된 날짜와 시간 복사
		Get_ClockSnapshot(&current_time);

		// LCD에 날짜와 시간 숫자만 출력
		Display_DateTime(&current_time);

		// SW1을 누르면 현재 값 확정
		if (Switch_Pressed(SW1_CONFIRM))
		{
			field++;
		}

		// LCD 갱신 간격
		_delay_ms(30);
	}

	// 최종 날짜와 시간 복사
	Get_ClockSnapshot(&current_time);

	// 날짜와 시간 유효성 확인
	if (!DateTime_IsValid(&current_time))
	{
		DateTime_Reset();
	}

	// 확정된 날짜와 시간 출력
	Get_ClockSnapshot(&current_time);
	Display_DateTime(&current_time);

	// SW2가 눌릴 때까지 시간 정지
	while (!Switch_Pressed(SW2_START))
	{
		Get_ClockSnapshot(&current_time);
		Display_DateTime(&current_time);

		_delay_ms(30);
	}

	// 설정한 시간부터 Timer0 시작
	Timer0_Start();

	while (1)
	{
		// 현재 날짜와 시간 복사
		Get_ClockSnapshot(&current_time);

		// LCD에 날짜와 시간 출력
		Display_DateTime(&current_time);

		// LCD 화면 갱신 간격
		_delay_ms(20);
	}

	return 0;
}