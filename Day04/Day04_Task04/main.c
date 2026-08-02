#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define BAUD_RATE 9600UL
#define UBRR_VALUE ((F_CPU / (16UL * BAUD_RATE)) - 1)

#define ADC_CHANNEL 7
#define FILTER_SIZE 10

typedef struct
{
	uint16_t adc;
	uint16_t distance_x10;
} DistancePoint;

// 실제 측정값으로 수정할 거리 변환표
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

uint16_t filter_buffer[FILTER_SIZE];
uint32_t filter_sum = 0;
uint8_t filter_index = 0;
uint8_t filter_count = 0;

void JTAG_Disable(void)
{
	uint8_t value;

	// PF7을 ADC로 사용하기 위해 JTAG 비활성화
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
	// UART 통신 속도 설정
	UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
	UBRR0L = (uint8_t)UBRR_VALUE;

	// 일반 비동기 모드 설정
	UCSR0A = 0x00;

	// UART 송신 활성화
	UCSR0B = (1 << TXEN0);

	// 데이터 8비트, 패리티 없음, 정지 비트 1개
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void UART0_SendChar(char data)
{
	// 송신 버퍼가 빌 때까지 대기
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

	// 숫자를 원래 순서로 출력
	while (index > 0)
	{
		index--;
		UART0_SendChar(buffer[index]);
	}
}

void ADC_Init(void)
{
	// PF7을 입력으로 설정
	DDRF &= ~(1 << DDF7);

	// PF7 내부 풀업 저항 비활성화
	PORTF &= ~(1 << PF7);

	// 기준전압 AVCC, ADC7 선택
	ADMUX = (1 << REFS0) | ADC_CHANNEL;

	// ADC 활성화, 분주비 128 설정
	ADCSRA =
	(1 << ADEN) |
	(1 << ADPS2) |
	(1 << ADPS1) |
	(1 << ADPS0);

	// ADC 안정화 대기
	_delay_ms(1);

	// 첫 번째 변환 시작
	ADCSRA |= (1 << ADSC);

	// 변환 완료 대기
	while (ADCSRA & (1 << ADSC));

	// 첫 번째 변환값 버리기
	(void)ADC;
}

uint16_t ADC_Read(void)
{
	// ADC 변환 시작
	ADCSRA |= (1 << ADSC);

	// ADC 변환 완료 대기
	while (ADCSRA & (1 << ADSC));

	// 10비트 ADC 결과 반환
	return ADC;
}

void MovingAverage_Init(void)
{
	uint8_t i;

	// 이동 평균 필터 배열 초기화
	for (i = 0; i < FILTER_SIZE; i++)
	{
		filter_buffer[i] = 0;
	}

	// 필터 변수 초기화
	filter_sum = 0;
	filter_index = 0;
	filter_count = 0;
}

uint16_t MovingAverage_Filter(uint16_t raw_value)
{
	// 버퍼가 가득 찬 경우 기존 값 제거
	if (filter_count >= FILTER_SIZE)
	{
		filter_sum -= filter_buffer[filter_index];
	}
	else
	{
		filter_count++;
	}

	// 새로운 RAW 값 저장
	filter_buffer[filter_index] = raw_value;

	// 새로운 RAW 값을 합계에 추가
	filter_sum += raw_value;

	// 다음 저장 위치로 이동
	filter_index++;

	// 배열 끝에서 처음으로 이동
	if (filter_index >= FILTER_SIZE)
	{
		filter_index = 0;
	}

	// 현재 저장된 값들의 평균 반환
	return (uint16_t)((filter_sum + (filter_count / 2))
	/ filter_count);
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
			distance_near =
			distance_table[i].distance_x10;

			distance_far =
			distance_table[i + 1].distance_x10;

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

	// 거리 구간을 찾지 못한 경우
	return -1;
}

void PrintMeasurement(
uint16_t raw_value,
uint16_t filtered_value,
int16_t distance_x10)
{
	// 필터 미적용 ADC 값 출력
	UART0_SendString("RAW: ");
	UART0_SendUInt16(raw_value);

	// 필터 적용 ADC 값 출력
	UART0_SendString(" | FILTERED: ");
	UART0_SendUInt16(filtered_value);

	// 거리 출력
	UART0_SendString(" | DISTANCE: ");

	// 측정 가능 범위를 벗어난 경우
	if (distance_x10 < 0)
	{
		UART0_SendString("OUT OF RANGE");
	}
	else
	{
		UART0_SendUInt16((uint16_t)distance_x10 / 10);
		UART0_SendChar('.');
		UART0_SendChar(
		((uint16_t)distance_x10 % 10) + '0'
		);
		UART0_SendString("cm");
	}

	// 줄바꿈
	UART0_SendString("\r\n");
}

int main(void)
{
	uint16_t raw_adc;
	uint16_t filtered_adc;
	int16_t distance_x10;

	// PF7의 JTAG 기능 비활성화
	JTAG_Disable();

	// UART0 초기화
	UART0_Init();

	// ADC 초기화
	ADC_Init();

	// 이동 평균 필터 초기화
	MovingAverage_Init();

	// PSD 센서 안정화 대기
	_delay_ms(100);

	while (1)
	{
		// 필터 미적용 ADC 값 측정
		raw_adc = ADC_Read();

		// RAW 값에 이동 평균 필터 적용
		filtered_adc = MovingAverage_Filter(raw_adc);

		// 필터 적용 ADC 값으로 거리 계산
		distance_x10 = ADC_ToDistance(filtered_adc);

		// RAW, 필터값, 거리 출력
		PrintMeasurement(
		raw_adc,
		filtered_adc,
		distance_x10
		);

		// 100ms마다 측정
		_delay_ms(100);
	}

	return 0;
}