# Day04_Task05
> **광운대학교 로봇학부**
> **작성자:** 김도훈
> **제출일:** 2026년 8월 2일

---

## 1. 개요 (Overview)

본 과제는 ATmega128 마이크로컨트롤러의 USART 통신과 Timer1 PWM 기능을 이용하여 서보모터의 회전 각도를 제어하는 시스템을 구현하는 것을 목표로 함.

컴퓨터의 시리얼 터미널에서 0도부터 180도 사이의 목표 각도를 입력하면, ATmega128이 입력된 문자열을 숫자로 변환하고 Timer1의 PWM 출력값을 변경하여 서보모터를 해당 각도로 이동시킴.

숫자가 아닌 문자가 입력되거나 세 자리를 초과한 입력, 180도를 초과한 각도가 입력되면 오류 메시지를 출력하도록 예외 처리를 구현함.

### 핵심 목표

* USART0을 이용한 문자 및 숫자 데이터 송수신
* 문자열 형태로 입력된 각도를 정수로 변환
* Timer1의 Fast PWM 모드를 이용한 서보모터 제어
* 입력 각도를 PWM 펄스폭으로 변환
* 0도부터 180도 범위의 각도 제어
* 잘못된 문자 및 범위를 벗어난 각도 예외 처리
* 시리얼 터미널을 이용한 제어 결과 확인

---

## 2. 개발 환경 (Environment)

| 항목                 | 내용                                          |
| :----------------- | :------------------------------------------ |
| **MCU**            | ATmega128A (16MHz External Crystal)         |
| **IDE / Compiler** | Microchip Studio 7.0 / Microchip AVR GCC    |
| **Flasher Tool**   | USBISP / STK500                             |
| **언어**             | C Language                                  |
| **주요 부품**          | ATmega128 개발보드, 서보모터, USB to UART 모듈, 외부 전원 |

---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]                    [Target Component]

PB7 / OC1C          -------->  서보모터 PWM 신호선
PE0 / RXD0          <--------  USB to UART 모듈 TX
PE1 / TXD0          -------->  USB to UART 모듈 RX
VCC                 -------->  서보모터 전원
GND                 -------->  서보모터 및 UART 공통 GND
```

### 주요 회로 특징

* **전원:** ATmega128에는 5V DC 안정화 전원을 공급함.
* **서보모터 전원:** 서보모터의 소비 전류가 크므로 필요에 따라 별도의 안정적인 외부 전원을 사용함.
* **PWM 출력:** PB7의 OC1C 핀을 서보모터의 신호선에 연결함.
* **USART 통신:** USB to UART 모듈을 통해 컴퓨터의 시리얼 터미널과 데이터를 송수신함.
* **UART 연결:** ATmega128의 TXD0은 UART 모듈의 RX에, RXD0은 UART 모듈의 TX에 교차 연결함.
* **공통 접지:** ATmega128, 서보모터 전원, USB to UART 모듈의 GND를 서로 연결함.
* **PWM 주기:** 서보모터 제어를 위해 약 20ms 주기의 PWM 신호를 사용함.
* **주의사항:** 서보모터를 ATmega128의 출력 핀에서 직접 구동하지 않고, 전원 단자에 별도의 전원을 공급해야 함.

---

## 4. 프로젝트 구조 (Directory Structure)

> 구현부(.c), 선언부(.h)만 구조에 표기함.

```text
├─ Servo_UART_Control/
│   ├── main.c       # UART 입력, PWM 설정 및 서보모터 제어
└── README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### UART 통신 속도 설정

```c
#define BAUD_RATE 9600UL

#define UBRR_VALUE \
((F_CPU / (16UL * BAUD_RATE)) - 1)
```

ATmega128과 컴퓨터 사이의 USART0 통신 속도를 9600bps로 설정함.

16MHz CPU 클럭에서 일반 비동기 통신 모드를 사용하는 경우 UBRR 값은 다음과 같이 계산됨.

```text
UBRR = 16,000,000 ÷ (16 × 9,600) - 1
     ≒ 103
```

계산된 값을 `UBRR0H`와 `UBRR0L` 레지스터에 나누어 저장함.

### USART0 초기화

```c
void UART0_Init(void)
{
	UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
	UBRR0L = (uint8_t)UBRR_VALUE;

	UCSR0A = 0x00;

	UCSR0B = (1 << RXEN0) | (1 << TXEN0);

	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}
```

USART0을 이용하여 컴퓨터와 각도 데이터를 송수신할 수 있도록 초기화함.

| 설정 항목  |    설정값    |
| :----- | :-------: |
| 통신 방식  | 일반 비동기 통신 |
| 통신 속도  |  9600bps  |
| 데이터 비트 |    8비트    |
| 패리티 비트 |     없음    |
| 정지 비트  |    1비트    |
| 송신 기능  |    활성화    |
| 수신 기능  |    활성화    |

`RXEN0` 비트를 설정하여 USART0 수신 기능을 활성화하고, `TXEN0` 비트를 설정하여 송신 기능을 활성화함.

`UCSZ01`과 `UCSZ00` 비트를 설정하여 데이터 크기를 8비트로 구성함.

### UART 문자 송신

```c
void UART0_SendChar(char data)
{
	while (!(UCSR0A & (1 << UDRE0)));

	UDR0 = data;
}
```

`UDRE0` 비트를 확인하여 USART 송신 버퍼가 비워질 때까지 기다림.

송신 버퍼가 비워지면 `UDR0` 레지스터에 전송할 문자를 저장함.

### UART 문자 수신

```c
char UART0_ReceiveChar(void)
{
	while (!(UCSR0A & (1 << RXC0)));

	return UDR0;
}
```

`RXC0` 비트가 1이 될 때까지 기다려 새로운 문자가 수신되었는지 확인함.

문자가 수신되면 `UDR0` 레지스터의 값을 반환함.

이 함수는 문자가 수신될 때까지 프로그램이 기다리는 블로킹 방식으로 동작함.

### UART 문자열 송신

```c
void UART0_SendString(const char *string)
{
	while (*string != '\0')
	{
		UART0_SendChar(*string);
		string++;
	}
}
```

문자열의 끝을 나타내는 널 문자 `'\0'`을 만날 때까지 한 글자씩 USART0으로 전송함.

각도 입력 안내, 오류 메시지 및 적용 결과를 시리얼 터미널에 출력할 때 사용함.

### 정수 출력 함수

```c
void UART0_SendUInt16(uint16_t number)
{
	char buffer[5];
	uint8_t index = 0;

	if (number == 0)
	{
		UART0_SendChar('0');
		return;
	}

	while (number > 0)
	{
		buffer[index] = (number % 10) + '0';
		number /= 10;
		index++;
	}

	while (index > 0)
	{
		index--;
		UART0_SendChar(buffer[index]);
	}
}
```

서보모터에 적용된 각도를 USART0으로 출력하기 위해 정수를 문자열 형태로 변환함.

숫자를 10으로 나눈 나머지를 이용하여 각 자릿수를 구하면 일의 자리부터 저장되므로, 배열에 저장된 문자를 역순으로 출력함.

예를 들어 숫자 `135`는 다음 순서로 저장됨.

```text
저장 순서: '5', '3', '1'
출력 순서: '1', '3', '5'
```

`printf()`를 사용하지 않고 직접 숫자를 문자로 변환하여 프로그램 메모리 사용량을 줄임.

### 서보모터 PWM 설정값

```c
#define SERVO_MIN_PULSE 1200
#define SERVO_MAX_PULSE 4800
#define SERVO_INITIAL_ANGLE 0
```

서보모터의 각도에 따라 Timer1의 비교 일치값을 변경함.

| 설정값                   | 의미                 |
| :-------------------- | :----------------- |
| `SERVO_MIN_PULSE`     | 0도에 해당하는 PWM 비교값   |
| `SERVO_MAX_PULSE`     | 180도에 해당하는 PWM 비교값 |
| `SERVO_INITIAL_ANGLE` | 프로그램 시작 시 초기 각도    |

현재 코드의 초기 각도는 `0도`로 설정되어 있음.

### Timer1 PWM 초기화

```c
void Servo_Init(void)
{
	DDRB |= (1 << DDB7);

	TCCR1A =
	(1 << COM1C1) |
	(1 << WGM11);

	TCCR1B =
	(1 << WGM13) |
	(1 << WGM12) |
	(1 << CS11);

	ICR1 = 39999;

	OCR1C = 0;
}
```

PB7을 출력으로 설정하여 Timer1의 OC1C PWM 신호가 출력되도록 함.

Timer1은 ICR1을 TOP 값으로 사용하는 Fast PWM 모드로 설정함.

#### TCCR1A 설정

| 비트       |  설정 | 기능              |
| :------- | :-: | :-------------- |
| `COM1C1` |  1  | OC1C 비반전 PWM 출력 |
| `COM1C0` |  0  | 비반전 출력 구성       |
| `WGM11`  |  1  | Fast PWM 모드 설정  |

#### TCCR1B 설정

| 비트      |  설정 | 기능                        |
| :------ | :-: | :------------------------ |
| `WGM13` |  1  | ICR1을 TOP으로 사용하는 Fast PWM |
| `WGM12` |  1  | Fast PWM 모드 설정            |
| `CS11`  |  1  | 분주비 8 설정                  |

Timer1의 입력 클럭은 다음과 같음.

```text
Timer1 클럭 = 16MHz ÷ 8
             = 2MHz
```

Timer1의 한 카운트 시간은 다음과 같음.

```text
1카운트 시간 = 1 ÷ 2,000,000
              = 0.5µs
```

`ICR1`을 39999로 설정하여 Timer1이 0부터 39999까지 총 40000번 카운트하도록 함.

```text
PWM 주기 = 0.5µs × 40,000
         = 20,000µs
         = 20ms
```

따라서 서보모터 제어에 필요한 약 20ms 주기의 PWM 신호가 생성됨.

### 입력 각도를 PWM 값으로 변환

```c
void Servo_SetAngle(uint16_t angle)
{
	uint16_t pulse;

	pulse =
	SERVO_MIN_PULSE +
	((uint32_t)angle *
	(SERVO_MAX_PULSE - SERVO_MIN_PULSE) / 180);

	OCR1C = pulse;
}
```

입력된 0도부터 180도 사이의 각도를 `SERVO_MIN_PULSE`부터 `SERVO_MAX_PULSE` 사이의 PWM 비교값으로 변환함.

변환식은 다음과 같음.

```text
PWM 값
=
최소 PWM 값
+
각도 × (최대 PWM 값 - 최소 PWM 값) ÷ 180
```

각도에 따른 PWM 비교값은 다음과 같음.

| 입력 각도 | OCR1C 값 | HIGH 유지 시간 |
| :---: | :-----: | :--------: |
|   0도  |   1200  |   약 0.6ms  |
|  90도  |   3000  |   약 1.5ms  |
|  180도 |   4800  |   약 2.4ms  |

Timer1의 한 카운트 시간이 0.5µs이므로 OCR1C 값에 0.5µs를 곱하면 PWM HIGH 시간을 계산할 수 있음.

입력 각도가 커질수록 OCR1C 값이 증가하고, PWM 신호의 HIGH 유지 시간이 길어져 서보모터의 회전 각도가 변경됨.

### 각도 문자열 입력

```c
uint8_t ReadAngle(uint16_t *angle)
{
	char data;
	uint16_t value = 0;
	uint8_t digit_count = 0;
	uint8_t invalid = 0;

	while (1)
	{
		data = UART0_ReceiveChar();

		if (data == '\r' || data == '\n')
		{
			if (digit_count > 0 || invalid)
			{
				UART0_SendString("\r\n");
				break;
			}

			continue;
		}

		UART0_SendChar(data);

		if (data >= '0' && data <= '9')
		{
			if (digit_count >= 3)
			{
				invalid = 1;
				continue;
			}

			value = value * 10 + (data - '0');
			digit_count++;
		}
		else
		{
			invalid = 1;
		}
	}

	if (digit_count == 0)
	{
		return 0;
	}

	if (invalid)
	{
		return 0;
	}

	*angle = value;

	return 1;
}
```

시리얼 터미널에서 Enter 키를 입력할 때까지 문자를 한 글자씩 수신함.

수신된 문자는 사용자가 확인할 수 있도록 다시 시리얼 터미널로 전송함.

숫자 문자가 입력되면 다음 식을 이용하여 정수값으로 변환함.

```text
새로운 값 = 기존 값 × 10 + 입력 숫자
```

예를 들어 `1`, `3`, `5`가 순서대로 입력되면 다음과 같이 계산됨.

```text
첫 번째 입력: 0 × 10 + 1 = 1
두 번째 입력: 1 × 10 + 3 = 13
세 번째 입력: 13 × 10 + 5 = 135
```

최종적으로 정수값 135가 생성됨.

### 입력 예외 처리

```c
if (data >= '0' && data <= '9')
{
	if (digit_count >= 3)
	{
		invalid = 1;
		continue;
	}

	value = value * 10 + (data - '0');
	digit_count++;
}
else
{
	invalid = 1;
}
```

각도는 0부터 180까지이므로 최대 세 자리 숫자만 입력할 수 있도록 제한함.

숫자가 아닌 문자가 입력되거나 세 자리를 초과하면 `invalid` 값을 1로 설정함.

입력이 끝난 후 다음 조건을 확인함.

```c
if (digit_count == 0)
{
	return 0;
}

if (invalid)
{
	return 0;
}
```

숫자가 하나도 입력되지 않았거나 잘못된 문자가 포함된 경우 `0`을 반환하여 잘못된 입력임을 알림.

정상적으로 숫자만 입력된 경우 포인터를 통해 계산한 각도를 전달하고 `1`을 반환함.

### 각도 범위 확인

```c
if (angle > 180)
{
	UART0_SendString(
	"ERROR: ANGLE MUST BE 0~180\r\n"
	);

	continue;
}
```

문자열 입력이 정상적으로 숫자로 변환되었더라도 입력 각도가 180도를 초과하면 서보모터 제어 범위를 벗어난 것으로 판단함.

이 경우 서보모터를 움직이지 않고 오류 메시지를 출력한 후 새로운 입력을 기다림.

### 메인 반복문

```c
while (1)
{
	UART0_SendString(
	"\r\nEnter Angle 0~180: "
	);

	input_result = ReadAngle(&angle);

	if (input_result == 0)
	{
		UART0_SendString(
		"ERROR: INVALID INPUT\r\n"
		);
		continue;
	}

	if (angle > 180)
	{
		UART0_SendString(
		"ERROR: ANGLE MUST BE 0~180\r\n"
		);
		continue;
	}

	Servo_SetAngle(angle);

	UART0_SendString("SERVO ANGLE: ");
	UART0_SendUInt16(angle);
	UART0_SendString(" degree\r\n");
}
```

사용자에게 목표 각도 입력을 요청하고 `ReadAngle()` 함수를 이용하여 문자열을 정수 각도로 변환함.

잘못된 입력이나 범위를 벗어난 입력은 오류 메시지를 출력하고 다시 입력을 받음.

정상적인 각도는 `Servo_SetAngle()` 함수에 전달하여 PWM 비교값으로 변환하고, 서보모터를 해당 각도로 이동시킴.

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오

1. 프로그램이 시작되면 USART0을 9600bps로 초기화함.
2. USART0의 송신 기능과 수신 기능을 모두 활성화함.
3. PB7을 Timer1의 OC1C PWM 출력 핀으로 설정함.
4. Timer1을 ICR1이 TOP 값인 Fast PWM 모드로 설정함.
5. Timer1의 분주비를 8로 설정함.
6. `ICR1`을 39999로 설정하여 약 20ms 주기의 PWM을 생성함.
7. 프로그램 시작 시 서보모터를 코드에 정의된 초기 각도인 0도로 이동시킴.
8. 서보모터가 초기 위치로 이동할 수 있도록 500ms 동안 대기함.
9. 시리얼 터미널에 프로그램 시작 문구와 각도 입력 안내를 출력함.
10. 사용자가 0부터 180 사이의 각도를 입력하고 Enter 키를 누름.
11. 입력된 문자들이 숫자인지 확인함.
12. 입력된 각 문자들을 정수 각도로 변환함.
13. 숫자가 아닌 문자가 포함되면 `INVALID INPUT` 오류를 출력함.
14. 입력된 각도가 180도를 초과하면 각도 범위 오류를 출력함.
15. 정상적인 각도는 PWM 비교값으로 변환됨.
16. 변환된 값을 `OCR1C`에 저장하여 서보모터를 해당 각도로 이동시킴.
17. 시리얼 터미널에 적용된 각도를 출력함.
18. 이후 새로운 각도를 계속 입력받아 서보모터를 반복 제어함.

### 정상 입력 예시

```text
Servo Motor Control Start
Initial Angle: 0 degree

Enter Angle 0~180: 90
SERVO ANGLE: 90 degree

Enter Angle 0~180: 135
SERVO ANGLE: 135 degree

Enter Angle 0~180: 180
SERVO ANGLE: 180 degree
```

사용자가 `90`을 입력하면 Timer1의 `OCR1C`에는 약 3000이 저장됨.

PWM 신호의 HIGH 시간은 약 1.5ms가 되고, 서보모터는 약 90도 위치로 이동함.

### 동작 사진 / 영상

|           동작 영상          |
| :----------------------: |
| https://drive.google.com/drive/folders/1o64ErgWdkI4Dj_jRctpaSXNeld2crE1i |

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)

본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool)  | 활용 영역     | 세부 사용 목적 및 내용                              |
| :---------- | :-------- | :----------------------------------------- |
| **ChatGPT** | 개념 정리     | USART 통신, Timer1, Fast PWM 및 서보모터 제어 원리 참고 |
| **ChatGPT** | 코드 구성 참고  | UART 문자 입력, 문자열의 정수 변환 및 입력 예외 처리 방법 참고    |
| **ChatGPT** | PWM 계산 참고 | 0~180도 각도를 PWM 비교값으로 변환하는 계산 방법 참고         |
| **ChatGPT** | 문서 작성 참고  | 프로그램의 동작 과정과 주요 레지스터 설정 내용 정리              |

### AI 활용 및 검증 원칙

1. AI가 제공한 USART, Timer1 및 PWM 관련 개념을 이해한 후 직접 코드를 작성함.
2. ATmega128 데이터시트를 통해 Timer1과 USART0 레지스터의 설정값을 확인함.
3. 오실로스코프 또는 로직 분석기를 이용하여 PWM 주기와 펄스폭을 확인함.
4. 0도, 90도, 180도를 입력하여 서보모터의 실제 회전 위치를 확인함.
5. 서보모터의 실제 동작 범위에 맞게 최소 및 최대 PWM 값을 조정함.
6. 숫자가 아닌 문자, 세 자리를 초과한 입력, 180도를 초과한 입력에 대한 오류 처리를 확인함.
7. UART 터미널의 통신 속도와 데이터 형식을 코드 설정과 동일하게 구성함.
8. AI가 제시한 내용을 그대로 사용하지 않고 실제 하드웨어 환경과 코드 목적에 맞게 수정하고 검증함.
