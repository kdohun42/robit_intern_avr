#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>
#include "i2c_lcd.h"

#define DXL_ID                      1

#define DXL_DIR_PIN                 PE2

#define DXL_TORQUE_ENABLE_ADDR      64
#define DXL_PROFILE_VELOCITY_ADDR   112
#define DXL_GOAL_POSITION_ADDR      116

#define ADC_SAMPLE_COUNT            8
#define POSITION_DEADBAND           3

#define INITIAL_SPEED_INPUT         3

#define DXL_USART_UBRR              34
#define PC_USART_UBRR               103

void RS485_TransmitMode(void)
{
	// MAX485를 송신 모드로 설정
	PORTE |= (1 << DXL_DIR_PIN);
}

void RS485_ReceiveMode(void)
{
	// MAX485를 수신 모드로 설정
	PORTE &= ~(1 << DXL_DIR_PIN);
}

void Dynamixel_USART0_Init(void)
{
	// PE2를 MAX485 방향 제어 출력으로 설정
	DDRE |= (1 << DXL_DIR_PIN);

	// MAX485를 수신 모드로 초기화
	RS485_ReceiveMode();

	// USART0 더블 스피드 모드 설정
	UCSR0A = (1 << U2X0);

	// USART0 통신 속도를 57600bps로 설정
	UBRR0H = (uint8_t)(DXL_USART_UBRR >> 8);
	UBRR0L = (uint8_t)DXL_USART_UBRR;

	// USART0 송신과 수신 활성화
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);

	// 데이터 8비트, 패리티 없음, 정지 비트 1개
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void PC_USART1_Init(void)
{
	// USART1 일반 속도 모드 설정
	UCSR1A = 0x00;

	// USART1 통신 속도를 9600bps로 설정
	UBRR1H = (uint8_t)(PC_USART_UBRR >> 8);
	UBRR1L = (uint8_t)PC_USART_UBRR;

	// USART1 송신과 수신 활성화
	UCSR1B = (1 << RXEN1) | (1 << TXEN1);

	// 데이터 8비트, 패리티 없음, 정지 비트 1개
	UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
}

void USART0_SendByte(uint8_t data)
{
	// USART0 송신 버퍼가 빌 때까지 대기
	while (!(UCSR0A & (1 << UDRE0)));

	// 데이터 전송
	UDR0 = data;
}

void USART0_FlushReceive(void)
{
	volatile uint8_t dummy;

	// USART0에 남아 있는 수신 데이터 제거
	while (UCSR0A & (1 << RXC0))
	{
		dummy = UDR0;
	}

	(void)dummy;
}

uint8_t USART1_DataAvailable(void)
{
	// PC에서 데이터가 수신되었는지 확인
	if (UCSR1A & (1 << RXC1))
	{
		return 1;
	}

	return 0;
}

uint8_t USART1_ReceiveByte(void)
{
	// USART1에서 수신한 데이터 반환
	return UDR1;
}

void ADC_Init(void)
{
	// PF0을 ADC 입력으로 설정
	DDRF &= ~(1 << DDF0);

	// PF0 내부 풀업 저항 비활성화
	PORTF &= ~(1 << PF0);

	// AVCC 기준전압과 ADC0 선택
	ADMUX = (1 << REFS0);

	// ADC 활성화와 분주비 128 설정
	ADCSRA =
	(1 << ADEN) |
	(1 << ADPS2) |
	(1 << ADPS1) |
	(1 << ADPS0);
	_delay_ms(1);

	// 첫 번째 ADC 변환 시작
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	(void)ADC;
}

uint16_t ADC_Read(void)
{
	// ADC 변환 시작
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	return ADC;
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
	return (uint16_t)(sum / ADC_SAMPLE_COUNT);
}

uint16_t Dynamixel_UpdateCRC(
uint16_t crc_accum,
uint8_t *data,
uint16_t data_size)
{
	uint16_t i;
	uint8_t j;

	// 패킷 전체의 CRC 계산
	for (i = 0; i < data_size; i++)
	{
		crc_accum ^= ((uint16_t)data[i] << 8);

		for (j = 0; j < 8; j++)
		{
			if (crc_accum & 0x8000)
			{
				crc_accum =
				(uint16_t)((crc_accum << 1) ^ 0x8005);
			}
			else
			{
				crc_accum =
				(uint16_t)(crc_accum << 1);
			}
		}
	}

	return crc_accum;
}

void Dynamixel_SendWritePacket(
uint16_t address,
uint8_t *data,
uint8_t data_length)
{
	uint8_t body[16];
	uint8_t stuffed_body[20];
	uint8_t packet[32];

	uint8_t body_length;
	uint8_t stuffed_length;
	uint8_t packet_length;

	uint8_t i;
	uint16_t length;
	uint16_t crc;

	// Write 명령과 주소를 패킷 본문에 저장
	body[0] = 0x03;
	body[1] = (uint8_t)(address & 0xFF);
	body[2] = (uint8_t)(address >> 8);

	// 전송할 데이터를 패킷 본문에 저장
	for (i = 0; i < data_length; i++)
	{
		body[3 + i] = data[i];
	}

	body_length = 3 + data_length;
	stuffed_length = 0;

	// Protocol 2.0 Byte Stuffing 적용
	for (i = 0; i < body_length; i++)
	{
		stuffed_body[stuffed_length] = body[i];
		stuffed_length++;

		if (stuffed_length >= 3)
		{
			if (stuffed_body[stuffed_length - 3] == 0xFF &&
			stuffed_body[stuffed_length - 2] == 0xFF &&
			stuffed_body[stuffed_length - 1] == 0xFD)
			{
				stuffed_body[stuffed_length] = 0xFD;
				stuffed_length++;
			}
		}
	}

	// Length는 본문과 CRC 2바이트를 포함
	length = stuffed_length + 2;

	// Protocol 2.0 헤더 생성
	packet[0] = 0xFF;
	packet[1] = 0xFF;
	packet[2] = 0xFD;
	packet[3] = 0x00;
	packet[4] = DXL_ID;
	packet[5] = (uint8_t)(length & 0xFF);
	packet[6] = (uint8_t)(length >> 8);

	// Byte Stuffing이 적용된 본문 저장
	for (i = 0; i < stuffed_length; i++)
	{
		packet[7 + i] = stuffed_body[i];
	}

	// CRC 계산
	crc = Dynamixel_UpdateCRC(
	0,
	packet,
	(uint16_t)(7 + stuffed_length)
	);

	// CRC를 하위 바이트부터 저장
	packet[7 + stuffed_length] =
	(uint8_t)(crc & 0xFF);

	packet[8 + stuffed_length] =
	(uint8_t)(crc >> 8);

	packet_length = 9 + stuffed_length;

	// 이전에 수신된 Status Packet 제거
	USART0_FlushReceive();

	// MAX485를 송신 모드로 변경
	RS485_TransmitMode();

	_delay_us(10);

	// 전송 완료 플래그 초기화
	UCSR0A |= (1 << TXC0);

	// 패킷 전송
	for (i = 0; i < packet_length; i++)
	{
		USART0_SendByte(packet[i]);
	}

	// 마지막 비트 전송 완료까지 대기
	while (!(UCSR0A & (1 << TXC0)));

	// MAX485를 수신 모드로 변경
	RS485_ReceiveMode();
	_delay_ms(3);

	// 수신된 Status Packet 제거
	USART0_FlushReceive();
}

void Dynamixel_Write1Byte(
uint16_t address,
uint8_t value)
{
	uint8_t data[1];

	// 1바이트 데이터 준비
	data[0] = value;

	// Write 패킷 전송
	Dynamixel_SendWritePacket(address, data, 1);
}

void Dynamixel_Write4Byte(
uint16_t address,
uint32_t value)
{
	uint8_t data[4];

	// 4바이트 값을 Little Endian으로 분리
	data[0] = (uint8_t)(value & 0xFF);
	data[1] = (uint8_t)((value >> 8) & 0xFF);
	data[2] = (uint8_t)((value >> 16) & 0xFF);
	data[3] = (uint8_t)((value >> 24) & 0xFF);

	// Write 패킷 전송
	Dynamixel_SendWritePacket(address, data, 4);
}

uint32_t Convert_PCInputToSpeed(uint8_t input_number)
{
	uint32_t speed;

	// PC 입력 0부터 9를 속도 0부터 300으로 변환
	speed =
	((uint32_t)input_number * 300UL) / 9UL;

	return speed;
}

uint16_t AbsoluteDifference(
uint16_t value1,
uint16_t value2)
{
	// 두 값의 차이 절댓값 계산
	if (value1 >= value2)
	{
		return value1 - value2;
	}

	return value2 - value1;
}

void LCD_WriteLine(uint8_t row, char *text)
{
	char line[17];
	uint8_t i;

	// LCD 한 줄을 공백으로 초기화
	for (i = 0; i < 16; i++)
	{
		line[i] = ' ';
	}

	line[16] = '\0';

	// 출력 문자열을 LCD 배열에 복사
	i = 0;

	while (text[i] != '\0' && i < 16)
	{
		line[i] = text[i];
		i++;
	}

	// 지정한 LCD 줄에 출력
	i2c_lcd_goto_xy(row, 0);
	i2c_lcd_string(line);
}

void LCD_Display(
uint32_t target_speed,
uint16_t target_position)
{
	char line1[17];
	char line2[17];

	// 첫 번째 줄에 목표 속도 생성
	snprintf(
	line1,
	sizeof(line1),
	"SPEED: %3lu",
	(unsigned long)target_speed
	);

	// 두 번째 줄에 목표 위치 생성
	snprintf(
	line2,
	sizeof(line2),
	"POSITION: %4u",
	(unsigned int)target_position
	);

	// LCD에 목표 속도와 위치 출력
	LCD_WriteLine(0, line1);
	LCD_WriteLine(1, line2);
}

int main(void)
{
	uint16_t target_position;
	uint16_t last_position;

	uint32_t target_speed;

	uint8_t received_data;
	uint8_t speed_input;
	uint8_t force_position_send;

	// I2C LCD 초기화
	i2c_lcd_init();

	// 가변저항 ADC 초기화
	ADC_Init();

	// PC 통신용 USART1 초기화
	PC_USART1_Init();

	// Dynamixel 통신용 USART0 초기화
	Dynamixel_USART0_Init();

	// Dynamixel 전원과 통신 안정화 대기
	_delay_ms(500);

	// 초기 PC 입력값 3을 속도로 변환
	target_speed =
	Convert_PCInputToSpeed(INITIAL_SPEED_INPUT);

	// 가변저항의 초기 위치값 읽기
	target_position = ADC_ReadAverage();
	last_position = target_position;

	// LCD 초기값 출력
	LCD_Display(target_speed, target_position);

	// Dynamixel 토크 비활성화
	Dynamixel_Write1Byte(
	DXL_TORQUE_ENABLE_ADDR,
	0
	);

	// 초기 목표 속도 설정
	Dynamixel_Write4Byte(
	DXL_PROFILE_VELOCITY_ADDR,
	target_speed
	);

	// Dynamixel 토크 활성화
	Dynamixel_Write1Byte(
	DXL_TORQUE_ENABLE_ADDR,
	1
	);

	_delay_ms(100);

	// 가변저항의 초기 목표 위치 전송
	Dynamixel_Write4Byte(
	DXL_GOAL_POSITION_ADDR,
	target_position
	);

	force_position_send = 0;

	while (1)
	{
		// PC에서 문자가 수신되었는지 확인
		if (USART1_DataAvailable())
		{
			received_data = USART1_ReceiveByte();

			// 숫자 0부터 9만 처리
			if (received_data >= '0' &&
			received_data <= '9')
			{
				// ASCII 문자를 실제 숫자로 변환
				speed_input =
				(uint8_t)(received_data - '0');

				// PC 입력값을 목표 속도로 변환
				target_speed =
				Convert_PCInputToSpeed(speed_input);

				// 목표 속도를 Dynamixel에 전송
				Dynamixel_Write4Byte(
				DXL_PROFILE_VELOCITY_ADDR,
				target_speed
				);

				// 새로운 속도를 적용하기 위해 위치 재전송
				force_position_send = 1;
			}
		}

		// 가변저항 값으로 목표 위치 설정
		target_position = ADC_ReadAverage();

		// 위치 변화가 일정 값 이상이면 전송
		if (force_position_send ||
		AbsoluteDifference(
		target_position,
		last_position
		) >= POSITION_DEADBAND)
		{
			// 목표 위치를 Dynamixel에 전송
			Dynamixel_Write4Byte(
			DXL_GOAL_POSITION_ADDR,
			target_position
			);

			// 마지막 전송 위치 저장
			last_position = target_position;

			// 강제 위치 전송 해제
			force_position_send = 0;
		}

		// LCD에 목표 속도와 위치 출력
		LCD_Display(
		target_speed,
		target_position
		);

		// 반복 주기 설정
		_delay_ms(50);
	}

	return 0;
}