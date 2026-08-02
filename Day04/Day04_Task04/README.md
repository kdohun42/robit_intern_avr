# Day04_Task04

> **광운대학교 로봇학부**
> **작성자:** 김도훈
> **제출일:** 2026년 8월 2일

---

## 1. 개요 (Overview)

본 과제는 ATmega128 마이크로컨트롤러와 PSD 거리 센서를 이용하여 물체와 센서 사이의 거리를 측정하는 시스템을 구현하는 것을 목표로 함.

PSD 센서에서 출력되는 아날로그 전압을 ATmega128의 ADC7 채널을 통해 디지털 값으로 변환함. 측정된 ADC 값에는 센서의 잡음과 미세한 흔들림이 포함될 수 있으므로 최근 10개의 ADC 측정값을 이용하는 이동 평균 필터를 적용함.

필터 적용 전 ADC 값, 필터 적용 후 ADC 값, 필터값을 이용하여 계산한 거리를 USART0 통신을 통해 컴퓨터의 시리얼 터미널에 출력함.

PSD 센서의 출력 전압과 거리는 선형 관계가 아니므로 거리별 ADC 기준값을 배열로 저장하고, 두 기준값 사이를 선형 보간하여 거리를 계산함.

### 핵심 목표

* ADC7 채널을 이용한 PSD 센서 출력값 측정
* 이동 평균 필터를 이용한 ADC 측정값의 노이즈 감소
* 원형 배열 구조를 이용한 최근 측정값 관리
* 거리표와 선형 보간을 이용한 거리 계산
* USART0 통신을 이용한 측정 결과 출력
* 필터 적용 전후 ADC 값 비교
* 측정 가능 범위를 벗어난 경우 예외 처리
* PF7 사용을 위한 JTAG 기능 비활성화

---

## 2. 개발 환경 (Environment)

| 항목                 | 내용                                                       |
| :----------------- | :------------------------------------------------------- |
| **MCU**            | ATmega128A (16MHz External Crystal)                      |
| **IDE / Compiler** | Microchip Studio 7.0 / Microchip AVR GCC                 |
| **Flasher Tool**   | USBISP / STK500                                          |
| **언어**             | C Language                                               |
| **주요 부품**          | ATmega128 개발보드, Sharp GP2Y0A21 PSD 거리 센서, USB to UART 모듈 |

---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]                    [Target Component]

PF7 / ADC7          <--------  PSD 센서 아날로그 출력
PE1 / TXD0          -------->  USB to UART 모듈 RX
VCC                 -------->  PSD 센서 VCC
GND                 -------->  PSD 센서 GND
```

### 주요 회로 특징

* **전원:** ATmega128과 PSD 센서에 5V DC 안정화 전원을 공급함.
* **센서 출력:** PSD 센서의 아날로그 출력 단자를 PF7의 ADC7 채널에 연결함.
* **USART 통신:** ATmega128의 USART0 송신 핀을 USB to UART 모듈의 RX 핀에 연결함.
* **공통 접지:** ATmega128, PSD 센서, USB to UART 모듈의 GND를 서로 연결함.
* **ADC 기준전압:** AVCC를 ADC 기준전압으로 사용함.
* **내부 풀업 저항:** 아날로그 입력에 영향을 주지 않도록 PF7의 내부 풀업 저항을 비활성화함.
* **JTAG 비활성화:** PF7을 ADC7 입력으로 사용하기 위해 JTAG 기능을 비활성화함.
* **주의사항:** PSD 센서 출력값은 거리와 선형적으로 비례하지 않으므로 거리표를 이용한 변환 과정이 필요함.

---

## 4. 프로젝트 구조 (Directory Structure)

> 구현부(.c), 선언부(.h)만 구조에 표기함.

```text
├─ PSD_Moving_Average/
│   ├── main.c       # ADC 측정, 이동 평균 필터, 거리 계산 및 UART 출력
└── README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### 거리 기준값 구조체

```c
typedef struct
{
	uint16_t adc;
	uint16_t distance_x10;
} DistancePoint;
```

PSD 센서의 ADC 기준값과 해당 거리를 하나의 데이터로 관리하기 위해 `DistancePoint` 구조체를 사용함.

`adc`에는 센서에서 측정되는 ADC 값을 저장하고, `distance_x10`에는 실제 거리의 10배 값을 저장함.

예를 들어 다음 데이터에서 `100`은 10.0cm를 의미함.

```c
{471, 100}
```

실수형을 사용하지 않고 정수 연산만으로 소수점 첫째 자리까지 표현하기 위해 실제 거리에 10을 곱하여 저장함.

### PSD 센서 거리 변환표

```c
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
```

PSD 센서의 거리별 ADC 기준값을 구조체 배열로 저장함.

| ADC 값 |   거리   |
| :---: | :----: |
|  471  | 10.0cm |
|  338  | 15.0cm |
|  266  | 20.0cm |
|  225  | 25.0cm |
|  194  | 30.0cm |
|  170  | 35.0cm |
|  153  | 40.0cm |
|  123  | 50.0cm |
|  102  | 60.0cm |
|   90  | 70.0cm |
|   82  | 80.0cm |

센서와 물체 사이의 거리가 가까워질수록 센서 출력 전압과 ADC 값이 커지며, 거리가 멀어질수록 ADC 값이 작아짐.

현재 거리표는 기본 기준값으로 작성되어 있으며, 실제 센서 측정 결과에 따라 ADC 값을 보정할 수 있음.

### 거리표 크기 계산

```c
#define DISTANCE_TABLE_SIZE \
(sizeof(distance_table) / sizeof(distance_table[0]))
```

전체 배열의 크기를 배열 요소 하나의 크기로 나누어 거리표에 저장된 요소의 개수를 자동으로 계산함.

거리표에 데이터가 추가되거나 삭제되더라도 별도로 크기 값을 수정할 필요가 없음.

### 이동 평균 필터 변수

```c
#define FILTER_SIZE 10

uint16_t filter_buffer[FILTER_SIZE];
uint32_t filter_sum = 0;
uint8_t filter_index = 0;
uint8_t filter_count = 0;
```

최근 10개의 ADC 값을 저장하고 평균을 계산하기 위해 이동 평균 필터 변수를 사용함.

| 변수              | 역할                    |
| :-------------- | :-------------------- |
| `filter_buffer` | 최근 ADC 값 10개 저장       |
| `filter_sum`    | 현재 버퍼에 저장된 ADC 값의 합   |
| `filter_index`  | 새로운 값을 저장할 배열 위치      |
| `filter_count`  | 현재까지 저장된 유효한 ADC 값 개수 |

매번 배열 전체를 다시 더하지 않고 기존 합계에서 가장 오래된 값을 빼고 새로운 값을 더하는 방식으로 평균을 계산함.

### JTAG 기능 비활성화

```c
void JTAG_Disable(void)
{
	uint8_t value;

	value = MCUCSR | (1 << JTD);

	__asm__ __volatile__(
	"out %0, %1" "\n\t"
	"out %0, %1"
	:
	: "I" (_SFR_IO_ADDR(MCUCSR)), "r" (value)
	);
}
```

ATmega128의 PF4부터 PF7은 JTAG 기능과 함께 사용되는 핀임.

PF7을 ADC7 입력으로 사용하기 위해 `MCUCSR` 레지스터의 `JTD` 비트를 설정하여 JTAG 기능을 비활성화함.

`JTD` 비트는 일정한 시간 안에 두 번 연속으로 설정해야 하므로 인라인 어셈블리 명령을 이용하여 같은 값을 두 번 연속으로 출력함.

JTAG 기능을 비활성화하지 않으면 PF7에서 PSD 센서의 아날로그 출력값을 정상적으로 측정하지 못할 수 있음.

### USART0 초기화

```c
void UART0_Init(void)
{
	UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
	UBRR0L = (uint8_t)UBRR_VALUE;

	UCSR0A = 0x00;

	UCSR0B = (1 << TXEN0);

	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}
```

USART0을 이용하여 측정 결과를 컴퓨터의 시리얼 터미널로 전송함.

```c
#define BAUD_RATE 9600UL
#define UBRR_VALUE ((F_CPU / (16UL * BAUD_RATE)) - 1)
```

16MHz CPU 클럭에서 9600bps를 사용하기 위한 UBRR 값은 다음과 같이 계산됨.

```text
UBRR = 16,000,000 ÷ (16 × 9,600) - 1
     ≒ 103
```

USART0의 통신 조건은 다음과 같음.

| 설정 항목  |   설정값   |
| :----- | :-----: |
| 통신 방식  |  비동기 통신 |
| 통신 속도  | 9600bps |
| 데이터 비트 |   8비트   |
| 패리티 비트 |    없음   |
| 정지 비트  |   1비트   |
| 활성화 기능 |    송신   |

`TXEN0` 비트를 1로 설정하여 송신 기능을 활성화함.

`UCSZ01`과 `UCSZ00`을 1로 설정하여 한 번에 전송하는 데이터 크기를 8비트로 설정함.

### UART 문자 및 문자열 전송

```c
void UART0_SendChar(char data)
{
	while (!(UCSR0A & (1 << UDRE0)));

	UDR0 = data;
}
```

`UDRE0` 비트를 확인하여 USART 송신 버퍼가 비워질 때까지 기다린 후 `UDR0`에 문자를 저장하여 전송함.

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

문자열의 끝을 나타내는 널 문자 `'\0'`을 만날 때까지 한 문자씩 전송함.

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

정수값을 USART로 전송하기 위해 숫자를 문자로 변환함.

숫자를 10으로 나눈 나머지를 이용하여 일의 자리부터 차례로 배열에 저장함. 저장된 자릿수는 반대 순서이므로 배열의 마지막 값부터 역순으로 출력함.

`printf()`를 사용하지 않고 직접 변환하여 프로그램의 메모리 사용량을 줄임.

### ADC 초기화

```c
void ADC_Init(void)
{
	DDRF &= ~(1 << DDF7);
	PORTF &= ~(1 << PF7);

	ADMUX = (1 << REFS0) | ADC_CHANNEL;

	ADCSRA =
	(1 << ADEN) |
	(1 << ADPS2) |
	(1 << ADPS1) |
	(1 << ADPS0);

	_delay_ms(1);

	ADCSRA |= (1 << ADSC);

	while (ADCSRA & (1 << ADSC));

	(void)ADC;
}
```

PF7을 입력으로 설정하고 내부 풀업 저항을 비활성화함.

```c
ADMUX = (1 << REFS0) | ADC_CHANNEL;
```

`REFS0` 비트를 1로 설정하여 AVCC를 ADC 기준전압으로 사용함.

`ADC_CHANNEL`은 7로 정의되어 있으므로 `MUX4~MUX0`은 `00111`이 되어 ADC7 채널이 선택됨.

`ADLAR` 비트는 설정하지 않았으므로 ADC 결과는 오른쪽 정렬 방식으로 저장됨.

```c
ADCSRA =
(1 << ADEN) |
(1 << ADPS2) |
(1 << ADPS1) |
(1 << ADPS0);
```

`ADEN` 비트를 1로 설정하여 ADC를 활성화하고, `ADPS2~ADPS0`을 모두 1로 설정하여 분주비 128을 사용함.

```text
ADC 클럭 = 16MHz ÷ 128
         = 125kHz
```

ADC 초기화 직후의 첫 번째 변환 결과는 불안정할 수 있으므로 첫 번째 측정 결과는 사용하지 않고 버림.

### ADC 값 읽기

```c
uint16_t ADC_Read(void)
{
	ADCSRA |= (1 << ADSC);

	while (ADCSRA & (1 << ADSC));

	return ADC;
}
```

`ADSC` 비트를 1로 설정하여 ADC 변환을 시작함.

변환이 완료되면 `ADSC` 비트가 자동으로 0이 되므로 해당 비트가 0이 될 때까지 기다림.

변환이 완료되면 ADC 데이터 레지스터 값을 반환함. 반환되는 값의 범위는 0부터 1023까지임.

### 이동 평균 필터 초기화

```c
void MovingAverage_Init(void)
{
	uint8_t i;

	for (i = 0; i < FILTER_SIZE; i++)
	{
		filter_buffer[i] = 0;
	}

	filter_sum = 0;
	filter_index = 0;
	filter_count = 0;
}
```

이동 평균 필터를 사용하기 전에 필터 배열의 모든 값을 0으로 초기화함.

ADC 값의 합계, 배열 저장 위치, 현재 저장된 값의 개수도 모두 0으로 초기화함.

### 이동 평균 필터 적용

```c
uint16_t MovingAverage_Filter(uint16_t raw_value)
{
	if (filter_count >= FILTER_SIZE)
	{
		filter_sum -= filter_buffer[filter_index];
	}
	else
	{
		filter_count++;
	}

	filter_buffer[filter_index] = raw_value;

	filter_sum += raw_value;

	filter_index++;

	if (filter_index >= FILTER_SIZE)
	{
		filter_index = 0;
	}

	return (uint16_t)((filter_sum + (filter_count / 2))
	/ filter_count);
}
```

현재 측정된 ADC 원본값을 이동 평균 필터에 입력함.

필터 버퍼가 아직 가득 차지 않았다면 `filter_count`를 증가시키고 현재까지 저장된 값만으로 평균을 계산함.

필터 버퍼에 10개의 값이 모두 저장된 이후에는 새로운 값을 저장하기 전에 현재 위치에 있는 가장 오래된 값을 합계에서 제거함.

그 후 새로운 ADC 값을 배열과 합계에 추가함.

```text
새로운 합계
=
기존 합계
-
가장 오래된 ADC 값
+
새로운 ADC 값
```

`filter_index`가 배열의 마지막 위치를 지나면 다시 0으로 변경함. 이를 통해 배열을 원형 버퍼처럼 반복하여 사용함.

평균값은 다음과 같이 계산됨.

```text
필터값 = 현재 저장된 ADC 값의 합 ÷ 저장된 값의 개수
```

나누기 전에 `filter_count / 2`를 더하여 정수 나눗셈에서 반올림되도록 구현함.

### 이동 평균 필터 동작 예시

최근 측정값이 다음과 같다고 가정함.

```text
200, 204, 198, 202, 201
```

평균값은 다음과 같이 계산됨.

```text
필터값 = (200 + 204 + 198 + 202 + 201) ÷ 5
       = 201
```

원본 ADC 값은 매번 조금씩 변하지만, 이동 평균값은 여러 측정값의 평균이므로 비교적 안정적인 값을 유지함.

필터 크기가 10이므로 초기 10회 측정까지는 현재까지 저장된 값만 사용하며, 10회 이후부터는 가장 오래된 값을 제거하면서 최근 10개의 값만 사용함.

### ADC 값을 거리로 변환

```c
int16_t ADC_ToDistance(uint16_t adc_value)
{
	uint8_t i;
	uint16_t adc_high;
	uint16_t adc_low;
	uint16_t distance_near;
	uint16_t distance_far;
	uint32_t distance_difference;

	if (adc_value > distance_table[0].adc ||
	adc_value < distance_table[DISTANCE_TABLE_SIZE - 1].adc)
	{
		return -1;
	}

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

			distance_difference =
			(uint32_t)(adc_high - adc_value) *
			(distance_far - distance_near);

			distance_difference /=
			(adc_high - adc_low);

			return distance_near +
			(uint16_t)distance_difference;
		}
	}

	return -1;
}
```

필터가 적용된 ADC 값을 거리로 변환함.

ADC 값이 거리표의 최댓값인 471보다 크거나 최솟값인 82보다 작으면 측정 가능 범위를 벗어난 것으로 판단하여 `-1`을 반환함.

정상 범위의 ADC 값은 반복문을 이용하여 어느 두 기준점 사이에 위치하는지 검색함.

두 기준점 사이의 실제 거리는 선형 보간을 사용하여 계산함.

```text
거리 증가값
=
(가까운 기준 ADC - 측정 ADC)
×
(먼 거리 - 가까운 거리)
÷
(가까운 기준 ADC - 먼 기준 ADC)
```

계산된 거리 증가값을 가까운 기준 거리에 더하여 최종 거리를 구함.

예를 들어 필터 ADC 값이 338과 266 사이에 있다면 거리는 15.0cm와 20.0cm 사이의 값으로 계산됨.

### 측정 결과 출력

```c
void PrintMeasurement(
uint16_t raw_value,
uint16_t filtered_value,
int16_t distance_x10)
{
	UART0_SendString("RAW: ");
	UART0_SendUInt16(raw_value);

	UART0_SendString(" | FILTERED: ");
	UART0_SendUInt16(filtered_value);

	UART0_SendString(" | DISTANCE: ");

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

	UART0_SendString("\r\n");
}
```

필터가 적용되지 않은 원본 ADC 값과 이동 평균 필터가 적용된 ADC 값을 함께 출력함.

이를 통해 이동 평균 필터 적용 전후의 값 변화를 직접 비교할 수 있음.

거리값이 `-1`인 경우 측정 가능 범위를 벗어난 것이므로 다음 문구를 출력함.

```text
OUT OF RANGE
```

정상 거리인 경우 소수점 첫째 자리까지 출력함.

```text
RAW: 268 | FILTERED: 265 | DISTANCE: 20.1cm
```

### 메인 반복문

```c
while (1)
{
	raw_adc = ADC_Read();

	filtered_adc = MovingAverage_Filter(raw_adc);

	distance_x10 = ADC_ToDistance(filtered_adc);

	PrintMeasurement(
	raw_adc,
	filtered_adc,
	distance_x10
	);

	_delay_ms(100);
}
```

PSD 센서의 ADC 값을 한 번 측정하여 `raw_adc`에 저장함.

원본 ADC 값을 이동 평균 필터에 입력하여 `filtered_adc`를 구함.

필터가 적용된 ADC 값을 이용하여 거리를 계산함. 원본값이 아닌 필터값으로 거리를 계산하기 때문에 센서값의 작은 흔들림으로 인해 거리 출력값이 빠르게 변하는 현상을 줄일 수 있음.

원본 ADC 값, 필터 ADC 값, 계산된 거리를 USART0으로 출력하고 100ms 후에 다시 측정함.

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오

1. 프로그램이 시작되면 PF7을 ADC7 채널로 사용하기 위해 JTAG 기능을 비활성화함.
2. USART0을 9600bps, 데이터 8비트, 패리티 없음, 정지 비트 1개로 초기화함.
3. PF7을 입력으로 설정하고 내부 풀업 저항을 비활성화함.
4. ADC 기준전압을 AVCC로 설정하고 ADC7 채널을 선택함.
5. ADC 분주비를 128로 설정하여 ADC 클럭을 125kHz로 구성함.
6. 이동 평균 필터 배열과 관련 변수를 0으로 초기화함.
7. PSD 센서의 출력이 안정화될 수 있도록 100ms 동안 대기함.
8. PSD 센서의 아날로그 출력값을 ADC로 측정함.
9. 필터가 적용되지 않은 측정값을 `RAW` 값으로 저장함.
10. RAW 값을 이동 평균 필터에 입력함.
11. 최근 최대 10개의 ADC 측정값을 이용하여 평균값을 계산함.
12. 필터가 적용된 ADC 값을 거리 변환표와 비교함.
13. 해당 ADC 값이 포함된 두 기준점 사이의 거리를 선형 보간하여 계산함.
14. RAW 값, 필터값, 계산된 거리를 USART0을 통해 시리얼 터미널에 출력함.
15. 측정 가능 범위를 벗어난 경우 `OUT OF RANGE`를 출력함.
16. 100ms마다 위 과정을 반복함.

### 이동 평균 필터 적용 전후 출력 예시

```text
RAW: 268 | FILTERED: 268 | DISTANCE: 19.9cm
RAW: 261 | FILTERED: 265 | DISTANCE: 20.1cm
RAW: 270 | FILTERED: 266 | DISTANCE: 20.0cm
RAW: 263 | FILTERED: 265 | DISTANCE: 20.1cm
RAW: 269 | FILTERED: 266 | DISTANCE: 20.0cm
```

RAW 값은 센서의 노이즈로 인해 261부터 270 사이에서 계속 변할 수 있음.

이동 평균 필터가 적용된 값은 최근 측정값들의 평균을 사용하므로 RAW 값보다 변화 폭이 작고 안정적인 값을 출력함.

### 거리 측정 결과 예시

```text
RAW: 340 | FILTERED: 338 | DISTANCE: 15.0cm
RAW: 269 | FILTERED: 266 | DISTANCE: 20.0cm
RAW: 197 | FILTERED: 194 | DISTANCE: 30.0cm
RAW: 155 | FILTERED: 153 | DISTANCE: 40.0cm
```

필터값이 거리표의 기준값과 일치하는 경우 해당 기준 거리가 그대로 출력됨.

필터값이 두 기준값 사이에 있는 경우 선형 보간을 통해 중간 거리가 계산됨.

```text
RAW: 247 | FILTERED: 246 | DISTANCE: 22.4cm
```

### 측정 범위 초과 출력 예시

```text
RAW: 78 | FILTERED: 80 | DISTANCE: OUT OF RANGE
```

필터 적용 ADC 값이 거리표의 최솟값인 82보다 작거나 최댓값인 471보다 큰 경우 측정 가능 범위를 벗어난 것으로 판단함.

### 동작 사진 / 영상

|           동작 영상          |
| :----------------------: |
| https://drive.google.com/drive/folders/1o64ErgWdkI4Dj_jRctpaSXNeld2crE1i |

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)

본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool)  | 활용 영역    | 세부 사용 목적 및 내용                           |
| :---------- | :------- | :-------------------------------------- |
| **ChatGPT** | 개념 정리    | ADC, USART, JTAG 비활성화 및 PSD 센서 동작 원리 참고 |
| **ChatGPT** | 필터 구성 참고 | 이동 평균 필터, 원형 배열 및 누적 합계 계산 방법 참고        |
| **ChatGPT** | 거리 계산 참고 | 거리 변환표와 선형 보간을 이용한 거리 계산 방법 참고          |
| **ChatGPT** | 문서 작성 참고 | 프로그램의 동작 과정과 주요 레지스터 설정 내용 정리           |

### AI 활용 및 검증 원칙

1. AI가 제공한 ADC, USART, 이동 평균 필터 관련 개념을 이해한 후 직접 코드를 작성함.
2. ATmega128 데이터시트를 통해 ADC와 USART 관련 레지스터의 설정값을 확인함.
3. 이동 평균 필터 적용 전후의 ADC 값을 시리얼 터미널에서 직접 비교함.
4. 필터값을 사용했을 때 거리 출력의 흔들림이 감소하는지 확인함.
5. PSD 센서와 물체 사이의 실제 거리를 측정하여 계산 결과와 비교함.
6. 실제 측정 결과에 따라 거리 변환표의 ADC 기준값을 수정하고 보정함.
7. 측정 범위를 벗어난 경우 `OUT OF RANGE` 문구가 정상적으로 출력되는지 확인함.
8. AI가 제시한 내용을 그대로 사용하지 않고 실제 하드웨어 환경과 코드 목적에 맞게 수정하고 검증함.
