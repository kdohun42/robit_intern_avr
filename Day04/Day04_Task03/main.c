#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define BAUD_RATE 9600UL
#define UBRR_VALUE ((F_CPU / (16UL * BAUD_RATE)) - 1)

#define ADC_CHANNEL 7
#define ADC_SAMPLE_COUNT 16

typedef struct
{
	uint16_t adc;
	uint16_t distance_x10;
} DistancePoint;

// Sharp GP2Y0A21 데이터시트 기준 거리표
const DistancePoint distance_table[] =
{
	{471, 100},
	{338, 150},
	{266, 200},
	{225, 250},
	{194, 300},
	{170, 350},
	{153, 400},
	{123, 500},
	{102, 600},
	{90,  700},
	{82,  800}
};

#define DISTANCE_TABLE_SIZE \
(sizeof(distance_table) / sizeof(distance_table[0]))

void JTAG_Disable(void)
{
	uint8_t value;

	// PF7을 ADC7로 사용하기 위해 JTAG 비활성화
	value = MCUCSR | (1 << JTD);

	__asm__ __volatile__(
	"out %0, %1" "\n\t"
	"out %0, %1"
	:
	: "I" (_SFR_IO_ADDR(MCUCSR)), "r" (value)
	);
}

void UART0_Init(void)
{
	// USART0 통신 속도 설정
	UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
	UBRR0L = (uint8_t)UBRR_VALUE;

	// 일반 속도 비동기 통신 설정
	UCSR0A = 0x00;

	// 송신 기능 활성화
	UCSR0B = (1 << TXEN0);

	// 데이터 8비트, 패리티 없음, 정지 비트 1개
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void UART0_SendChar(char data)
{
	// 송신 버퍼가 비워질 때까지 대기
	while (!(UCSR0A & (1 << UDRE0)));

	// 문자 전송
	UDR0 = data;
}

void UART0_SendString(const char *string)
{
	// 문자열 끝까지 전송
	while (*string != '\0')
	{
		UART0_SendChar(*string);
		string++;
	}
}

void UART0_SendUInt16(uint16_t number)
{
	char buffer[5];
	uint8_t index = 0;

	// 숫자가 0인 경우 처리
	if (number == 0)
	{
		UART0_SendChar('0');
		return;
	}

	// 숫자를 문자로 변환
	while (number > 0)
	{
		buffer[index] = (number % 10) + '0';
		number /= 10;
		index++;
	}

	// 변환된 문자를 역순으로 출력
	while (index > 0)
	{
		index--;
		UART0_SendChar(buffer[index]);
	}
}

void UART0_SendVoltage(uint16_t voltage_mv)
{
	// 전압의 정수 부분 출력
	UART0_SendUInt16(voltage_mv / 1000);

	UART0_SendChar('.');

	// 전압의 소수점 세 자리 출력
	UART0_SendChar(((voltage_mv / 100) % 10) + '0');
	UART0_SendChar(((voltage_mv / 10) % 10) + '0');
	UART0_SendChar((voltage_mv % 10) + '0');
}

void ADC_Init(void)
{
	// PF7을 입력으로 설정
	DDRF &= ~(1 << DDF7);

	// PF7 내부 풀업 저항 비활성화
	PORTF &= ~(1 << PF7);

	// 기준전압 AVCC, 오른쪽 정렬, ADC7 선택
	ADMUX = (1 << REFS0) | ADC_CHANNEL;

	// ADC 활성화 및 분주비 128 설정
	ADCSRA =
	(1 << ADEN) |
	(1 << ADPS2) |
	(1 << ADPS1) |
	(1 << ADPS0);

	// ADC 안정화 대기
	_delay_ms(1);

	// 첫 번째 ADC 변환 시작
	ADCSRA |= (1 << ADSC);

	// 첫 번째 ADC 변환 완료 대기
	while (ADCSRA & (1 << ADSC));

	// 첫 번째 변환 결과 버리기
	(void)ADCL;
	(void)ADCH;
}

uint16_t ADC_Read(void)
{
	uint8_t adc_low;
	uint8_t adc_high;

	// ADC 변환 시작
	ADCSRA |= (1 << ADSC);

	// ADC 변환 완료 대기
	while (ADCSRA & (1 << ADSC));

	// ADC 하위 바이트 먼저 읽기
	adc_low = ADCL;

	// ADC 상위 바이트 읽기
	adc_high = ADCH;

	// 10비트 ADC 값으로 결합
	return ((uint16_t)adc_high << 8) | adc_low;
}

uint16_t ADC_ReadAverage(void)
{
	uint32_t sum = 0;
	uint8_t i;

	// ADC 값을 여러 번 읽어 합산
	for (i = 0; i < ADC_SAMPLE_COUNT; i++)
	{
		sum += ADC_Read();
	}

	// ADC 평균값 반환
	return (uint16_t)((sum + (ADC_SAMPLE_COUNT / 2))
	/ ADC_SAMPLE_COUNT);
}

int16_t ADC_ToDistance(uint16_t adc_value)
{
	uint8_t i;
	uint16_t adc_high;
	uint16_t adc_low;
	uint16_t distance_near;
	uint16_t distance_far;
	uint32_t distance_difference;

	// 거리표 범위를 벗어난 경우
	if (adc_value > distance_table[0].adc ||
	adc_value < distance_table[DISTANCE_TABLE_SIZE - 1].adc)
	{
		return -1;
	}

	// ADC 값이 포함된 구간 검색
	for (i = 0; i < DISTANCE_TABLE_SIZE - 1; i++)
	{
		adc_high = distance_table[i].adc;
		adc_low = distance_table[i + 1].adc;

		if (adc_value <= adc_high &&
		adc_value >= adc_low)
		{
			distance_near = distance_table[i].distance_x10;
			distance_far = distance_table[i + 1].distance_x10;

			// 두 기준점 사이의 거리 계산
			distance_difference =
			(uint32_t)(adc_high - adc_value) *
			(distance_far - distance_near);

			distance_difference /=
			(adc_high - adc_low);

			return distance_near +
			(uint16_t)distance_difference;
		}
	}

	// 해당 구간을 찾지 못한 경우
	return -1;
}

uint16_t ADC_ToVoltage(uint16_t adc_value)
{
	// ADC 값을 밀리볼트 단위로 변환
	return (uint16_t)(((uint32_t)adc_value * 5000UL + 511UL)
	/ 1023UL);
}

void PrintMeasurement(
uint16_t adc_value,
uint16_t voltage_mv,
int16_t distance_x10)
{
	// ADC 값 출력
	UART0_SendString("ADC: ");
	UART0_SendUInt16(adc_value);

	// 전압 출력
	UART0_SendString(", Voltage: ");
	UART0_SendVoltage(voltage_mv);
	UART0_SendString(" V");

	// ADC 값이 비정상적인 경우
	if (adc_value <= 5 || adc_value >= 1018)
	{
		UART0_SendString(", SENSOR ERROR");
	}
	// 거리 측정 범위를 벗어난 경우
	else if (distance_x10 < 0)
	{
		UART0_SendString(", Distance: OUT OF RANGE");
	}
	// 정상 거리 출력
	else
	{
		UART0_SendString(", Distance: ");
		UART0_SendUInt16((uint16_t)distance_x10 / 10);
		UART0_SendChar('.');
		UART0_SendChar(((uint16_t)distance_x10 % 10) + '0');
		UART0_SendString(" cm");
	}

	// 줄바꿈
	UART0_SendString("\r\n");
}

int main(void)
{
	uint16_t adc_value;
	uint16_t voltage_mv;
	int16_t distance_x10;

	// PF7의 JTAG 기능 비활성화
	JTAG_Disable();

	// USART0 초기화
	UART0_Init();

	// ADC 초기화
	ADC_Init();

	// PSD 센서 초기 안정화 대기
	_delay_ms(100);

	UART0_SendString("PSD Distance Measurement Start\r\n");

	while (1)
	{
		// ADC 평균값 측정
		adc_value = ADC_ReadAverage();

		// ADC 값을 전압으로 변환
		voltage_mv = ADC_ToVoltage(adc_value);

		// ADC 값을 거리로 변환
		distance_x10 = ADC_ToDistance(adc_value);

		// 측정값을 UART로 출력
		PrintMeasurement(
		adc_value,
		voltage_mv,
		distance_x10
		);

		// 500ms 측정 주기
		_delay_ms(500);
	}

	return 0;
}