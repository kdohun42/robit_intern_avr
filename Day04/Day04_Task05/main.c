#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define BAUD_RATE 9600UL
#define UBRR_VALUE ((F_CPU / (16UL * BAUD_RATE)) - 1)

#define SERVO_MIN_PULSE 1200
#define SERVO_MAX_PULSE 4800
#define SERVO_INITIAL_ANGLE 0

void UART0_Init(void)
{
	// UART 통신 속도 설정
	UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
	UBRR0L = (uint8_t)UBRR_VALUE;

	// 일반 비동기 통신 설정
	UCSR0A = 0x00;

	// UART 송신과 수신 활성화
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);

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

char UART0_ReceiveChar(void)
{
	// 문자가 수신될 때까지 대기
	while (!(UCSR0A & (1 << RXC0)));

	// 수신한 문자 반환
	return UDR0;
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

	// 숫자가 0이면 바로 출력
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

	// 문자를 역순으로 출력
	while (index > 0)
	{
		index--;
		UART0_SendChar(buffer[index]);
	}
}

void Servo_Init(void)
{
	// PB7을 PWM 출력으로 설정
	DDRB |= (1 << DDB7);

	// OC1C 비반전 출력과 Fast PWM 설정
	TCCR1A =
	(1 << COM1C1) |
	(1 << WGM11);

	// Fast PWM 모드와 분주비 8 설정
	TCCR1B =
	(1 << WGM13) |
	(1 << WGM12) |
	(1 << CS11);

	// PWM 주기를 20ms로 설정
	ICR1 = 39999;

	// 초기 PWM 출력을 0으로 설정
	OCR1C = 0;
}

void Servo_SetAngle(uint16_t angle)
{
	uint16_t pulse;

	// 입력 각도를 PWM 값으로 변환
	pulse =
	SERVO_MIN_PULSE +
	((uint32_t)angle *
	(SERVO_MAX_PULSE - SERVO_MIN_PULSE) / 180);

	// PB7에 PWM 출력
	OCR1C = pulse;
}

uint8_t ReadAngle(uint16_t *angle)
{
	char data;
	uint16_t value = 0;
	uint8_t digit_count = 0;
	uint8_t invalid = 0;

	// Enter를 입력할 때까지 반복
	while (1)
	{
		data = UART0_ReceiveChar();

		// Enter 입력 확인
		if (data == '\r' || data == '\n')
		{
			if (digit_count > 0 || invalid)
			{
				UART0_SendString("\r\n");
				break;
			}

			continue;
		}

		// 입력한 문자를 터미널에 출력
		UART0_SendChar(data);

		// 숫자 문자인지 확인
		if (data >= '0' && data <= '9')
		{
			// 최대 세 자리까지만 허용
			if (digit_count >= 3)
			{
				invalid = 1;
				continue;
			}

			// 문자 숫자를 실제 숫자로 변환
			value = value * 10 + (data - '0');
			digit_count++;
		}
		else
		{
			// 숫자가 아닌 문자가 들어온 경우
			invalid = 1;
		}
	}

	// 숫자를 입력하지 않은 경우
	if (digit_count == 0)
	{
		return 0;
	}

	// 잘못된 문자가 포함된 경우
	if (invalid)
	{
		return 0;
	}

	// 계산한 각도를 전달
	*angle = value;

	return 1;
}

int main(void)
{
	uint16_t angle;
	uint8_t input_result;

	// UART0 초기화
	UART0_Init();

	// 서보모터 PWM 초기화
	Servo_Init();

	// 초기 위치를 90도로 설정
	Servo_SetAngle(SERVO_INITIAL_ANGLE);

	// 서보모터가 이동할 시간 대기
	_delay_ms(500);

	UART0_SendString("Servo Motor Control Start\r\n");
	UART0_SendString("Initial Angle: 90 degree\r\n");

	while (1)
	{
		// 목표 각도 입력 안내
		UART0_SendString("\r\nEnter Angle 0~180: ");

		// UART로 각도 입력
		input_result = ReadAngle(&angle);

		// 잘못된 입력 처리
		if (input_result == 0)
		{
			UART0_SendString("ERROR: INVALID INPUT\r\n");
			continue;
		}

		// 180도를 초과한 경우
		if (angle > 180)
		{
			UART0_SendString("ERROR: ANGLE MUST BE 0~180\r\n");
			continue;
		}

		// 입력한 각도로 서보모터 이동
		Servo_SetAngle(angle);

		// 적용된 각도 출력
		UART0_SendString("SERVO ANGLE: ");
		UART0_SendUInt16(angle);
		UART0_SendString(" degree\r\n");
	}

	return 0;
}