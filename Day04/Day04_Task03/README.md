# ATmega128 과제 및 프로젝트 템플릿

> **광운대학교 로봇학부**
> **작성자:** 김도훈
> **제출일:** 2026년 8월 2일

---

## 1. 개요 (Overview)

본 과제는 ATmega128 마이크로컨트롤러와 Sharp GP2Y0A21 PSD 거리 센서를 이용하여 물체와 센서 사이의 거리를 측정하는 시스템을 구현하는 것을 목표로 함.

PSD 센서에서 출력되는 아날로그 전압을 ATmega128의 ADC7 채널로 측정하고, 측정된 ADC 값을 전압과 거리로 변환함. 변환된 ADC 값, 전압, 거리 정보를 USART0 통신을 통해 컴퓨터의 시리얼 터미널에 출력함.

센서의 출력 전압과 거리의 관계가 선형적이지 않기 때문에 데이터시트의 기준값을 거리표로 저장하고, 두 기준점 사이의 값을 선형 보간하여 거리를 계산함.

### 핵심 목표

* ADC7 채널을 이용한 PSD 센서 출력값 측정
* ADC 값을 실제 전압으로 변환
* 거리표와 선형 보간을 이용한 거리 계산
* 여러 ADC 측정값의 평균을 이용한 노이즈 감소
* USART0 통신을 이용한 측정 결과 출력
* 센서 오류 및 측정 범위 초과 상태 처리
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
* **PSD 센서 출력:** 센서의 아날로그 출력 단자를 ATmega128의 PF7/ADC7에 연결함.
* **USART 통신:** USART0의 송신 핀을 USB to UART 모듈의 수신 핀에 연결함.
* **공통 접지:** ATmega128, PSD 센서, USB to UART 모듈의 GND를 서로 연결함.
* **ADC 기준전압:** AVCC를 ADC 기준전압으로 사용함.
* **JTAG 비활성화:** PF7을 ADC7 입력으로 사용하기 위해 ATmega128의 JTAG 기능을 비활성화함.
* **주의사항:** PSD 센서의 출력은 거리와 선형적으로 비례하지 않으므로 단순 비례식 대신 거리표와 보간 계산을 사용해야 함.

---

## 4. 프로젝트 구조 (Directory Structure)

> 구현부(.c), 선언부(.h)만 구조에 표기함.

```text
├─ PSD_Distance_Measurement/
│   ├── main.c       # ADC 측정, 거리 계산 및 UART 출력
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

PSD 센서의 ADC 기준값과 해당 거리를 하나의 자료로 관리하기 위해 `DistancePoint` 구조체를 사용함.

`adc`에는 PSD 센서에서 측정되는 ADC 값을 저장하고, `distance_x10`에는 실제 거리의 10배 값을 저장함.

예를 들어 다음 값에서 `100`은 10.0cm를 의미함.

```c
{471, 100}
```

실수 자료형을 사용하지 않고 정수 연산으로 소수점 첫째 자리까지 표현하기 위해 거리에 10을 곱한 값을 사용함.

### PSD 센서 거리표

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

Sharp GP2Y0A21 데이터시트의 거리와 출력 전압 관계를 기준으로 ADC 값과 거리의 대응값을 배열에 저장함.

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

센서와 물체 사이의 거리가 가까울수록 출력 전압과 ADC 값이 커지고, 거리가 멀어질수록 ADC 값이 작아짐.

### 거리표 크기 계산

```c
#define DISTANCE_TABLE_SIZE \
(sizeof(distance_table) / sizeof(distance_table[0]))
```

전체 배열의 크기를 배열 요소 하나의 크기로 나누어 거리표에 저장된 데이터 개수를 자동으로 계산함.

거리표의 항목이 추가되거나 삭제되더라도 별도로 배열 크기를 수정할 필요가 없음.

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

ATmega128에서 PF4부터 PF7은 JTAG 기능과 함께 사용되는 핀임. PF7을 ADC7 입력으로 사용하기 위해 JTAG 기능을 비활성화함.

`MCUCSR` 레지스터의 `JTD` 비트는 짧은 시간 안에 두 번 연속으로 설정해야 하므로 인라인 어셈블리 명령어를 사용하여 같은 값을 연속으로 두 번 출력함.

JTAG 기능을 비활성화하지 않으면 PF7에서 PSD 센서의 아날로그 값을 정상적으로 측정하지 못할 수 있음.

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

USART0를 이용하여 PSD 센서의 측정 결과를 컴퓨터로 전송함.

통신 속도는 9600bps로 설정함.

```c
#define BAUD_RATE 9600UL

#define UBRR_VALUE \
((F_CPU / (16UL * BAUD_RATE)) - 1)
```

16MHz 클럭과 9600bps 통신 속도를 사용할 때 UBRR 값은 다음과 같이 계산됨.

```text
UBRR = 16,000,000 ÷ (16 × 9,600) - 1
     ≒ 103
```

USART 통신 설정은 다음과 같음.

| 설정 항목  |   설정값   |
| :----- | :-----: |
| 통신 방식  |  비동기 통신 |
| 통신 속도  | 9600bps |
| 데이터 비트 |   8비트   |
| 패리티 비트 |    없음   |
| 정지 비트  |   1비트   |
| 사용 기능  |  송신 기능  |

`TXEN0` 비트를 1로 설정하여 USART0 송신 기능을 활성화함.

`UCSZ01`과 `UCSZ00`을 1로 설정하여 데이터 길이를 8비트로 설정함.

### UART 문자 및 문자열 전송

```c
void UART0_SendChar(char data)
{
	while (!(UCSR0A & (1 << UDRE0)));

	UDR0 = data;
}
```

`UDRE0` 비트를 확인하여 USART 송신 버퍼가 비워질 때까지 기다린 후 `UDR0` 레지스터에 전송할 문자를 저장함.

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

문자열의 끝을 나타내는 널 문자 `'\0'`을 만날 때까지 한 글자씩 USART로 전송함.

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

정수값을 USART로 출력하기 위해 숫자의 각 자릿수를 문자로 변환함.

숫자를 10으로 나눈 나머지를 이용하여 각 자릿수를 구하면 일의 자리부터 저장되므로, 저장된 문자를 역순으로 전송함.

`printf()`를 사용하지 않고 직접 숫자를 문자로 변환하여 프로그램의 메모리 사용량을 줄임.

### 전압 출력 함수

```c
void UART0_SendVoltage(uint16_t voltage_mv)
{
	UART0_SendUInt16(voltage_mv / 1000);

	UART0_SendChar('.');

	UART0_SendChar(((voltage_mv / 100) % 10) + '0');
	UART0_SendChar(((voltage_mv / 10) % 10) + '0');
	UART0_SendChar((voltage_mv % 10) + '0');
}
```

밀리볼트 단위로 저장된 전압을 볼트 단위로 출력함.

예를 들어 전압값이 `2315mV`인 경우 다음과 같이 출력됨.

```text
2.315 V
```

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

	(void)ADCL;
	(void)ADCH;
}
```

PF7을 입력으로 설정하고 내부 풀업 저항을 비활성화함.

```c
ADMUX = (1 << REFS0) | ADC_CHANNEL;
```

`REFS0` 비트를 1로 설정하여 AVCC를 ADC 기준전압으로 사용함.

`ADC_CHANNEL`의 값은 7이므로 `MUX4~MUX0` 비트에 `00111`이 설정되어 ADC7 채널이 선택됨.

ADC 결과는 오른쪽 정렬 방식으로 저장됨.

```c
ADCSRA =
(1 << ADEN) |
(1 << ADPS2) |
(1 << ADPS1) |
(1 << ADPS0);
```

`ADEN` 비트를 설정하여 ADC 기능을 활성화하고, `ADPS2~ADPS0`을 모두 1로 설정하여 분주비 128을 사용함.

```text
ADC 클럭 = 16MHz ÷ 128
         = 125kHz
```

ADC 초기화 이후 첫 번째 측정 결과는 불안정할 수 있으므로 첫 번째 변환값을 읽은 후 사용하지 않고 버림.

### ADC 값 읽기

```c
uint16_t ADC_Read(void)
{
	uint8_t adc_low;
	uint8_t adc_high;

	ADCSRA |= (1 << ADSC);

	while (ADCSRA & (1 << ADSC));

	adc_low = ADCL;
	adc_high = ADCH;

	return ((uint16_t)adc_high << 8) | adc_low;
}
```

`ADSC` 비트를 1로 설정하여 ADC 변환을 시작함.

ADC 변환이 완료되면 `ADSC` 비트가 자동으로 0이 되므로, 해당 비트가 0이 될 때까지 기다림.

ADC 결과가 오른쪽 정렬되어 있으므로 반드시 `ADCL`을 먼저 읽고 이후 `ADCH`를 읽음.

두 레지스터의 값을 결합하여 0부터 1023 사이의 10비트 ADC 값으로 반환함.

### ADC 평균값 측정

```c
uint16_t ADC_ReadAverage(void)
{
	uint32_t sum = 0;
	uint8_t i;

	for (i = 0; i < ADC_SAMPLE_COUNT; i++)
	{
		sum += ADC_Read();
	}

	return (uint16_t)((sum + (ADC_SAMPLE_COUNT / 2))
	/ ADC_SAMPLE_COUNT);
}
```

PSD 센서의 출력값에 포함될 수 있는 노이즈와 흔들림을 줄이기 위해 ADC 값을 16번 측정함.

```c
#define ADC_SAMPLE_COUNT 16
```

16개의 ADC 값을 모두 더한 후 측정 횟수로 나누어 평균값을 계산함.

나누기 전에 `ADC_SAMPLE_COUNT / 2`를 더하여 정수 나눗셈 과정에서 반올림되도록 구현함.

### ADC 값을 전압으로 변환

```c
uint16_t ADC_ToVoltage(uint16_t adc_value)
{
	return (uint16_t)(((uint32_t)adc_value * 5000UL + 511UL)
	/ 1023UL);
}
```

ADC 기준전압이 5V이므로 ADC 값 0~1023을 0~5000mV 범위로 변환함.

변환식은 다음과 같음.

```text
전압(mV) = ADC 값 × 5000 ÷ 1023
```

예를 들어 ADC 값이 512인 경우 약 2502mV로 계산됨.

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

먼저 ADC 값이 거리표의 측정 범위에 포함되는지 확인함.

ADC 값이 471보다 크거나 82보다 작으면 거리표의 범위를 벗어난 값이므로 `-1`을 반환함.

ADC 값이 정상 범위에 포함되면 해당 ADC 값이 어느 두 기준점 사이에 있는지 반복문으로 검색함.

두 기준점 사이의 거리는 선형 보간을 이용하여 계산함.

```text
거리 증가값
=
(가까운 기준 ADC - 측정 ADC)
×
(먼 거리 - 가까운 거리)
÷
(가까운 기준 ADC - 먼 기준 ADC)
```

계산된 거리 증가값을 가까운 기준 거리와 더하여 최종 거리를 구함.

예를 들어 ADC 값이 338과 266 사이에 있다면, 거리는 15.0cm와 20.0cm 사이의 값으로 계산됨.

### 측정 결과 출력

```c
void PrintMeasurement(
uint16_t adc_value,
uint16_t voltage_mv,
int16_t distance_x10)
{
	UART0_SendString("ADC: ");
	UART0_SendUInt16(adc_value);

	UART0_SendString(", Voltage: ");
	UART0_SendVoltage(voltage_mv);
	UART0_SendString(" V");

	if (adc_value <= 5 || adc_value >= 1018)
	{
		UART0_SendString(", SENSOR ERROR");
	}
	else if (distance_x10 < 0)
	{
		UART0_SendString(", Distance: OUT OF RANGE");
	}
	else
	{
		UART0_SendString(", Distance: ");
		UART0_SendUInt16((uint16_t)distance_x10 / 10);
		UART0_SendChar('.');
		UART0_SendChar(((uint16_t)distance_x10 % 10) + '0');
		UART0_SendString(" cm");
	}

	UART0_SendString("\r\n");
}
```

ADC 값, 센서 출력 전압, 계산된 거리를 USART0을 통해 출력함.

ADC 값이 5 이하이거나 1018 이상인 경우 센서 단선, 잘못된 연결 또는 비정상적인 입력 상태일 가능성이 있으므로 다음 문구를 출력함.

```text
SENSOR ERROR
```

ADC 값은 정상적이지만 거리표 범위를 벗어난 경우 다음 문구를 출력함.

```text
Distance: OUT OF RANGE
```

정상 범위에서는 거리를 소수점 첫째 자리까지 출력함.

### 메인 반복문

```c
while (1)
{
	adc_value = ADC_ReadAverage();

	voltage_mv = ADC_ToVoltage(adc_value);

	distance_x10 = ADC_ToDistance(adc_value);

	PrintMeasurement(
	adc_value,
	voltage_mv,
	distance_x10
	);

	_delay_ms(500);
}
```

ADC 값을 16번 측정하여 평균값을 구한 후, ADC 값을 전압과 거리로 변환함.

변환된 결과는 USART0을 통해 출력하며, 500ms마다 새로운 값을 측정함.

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오

1. 프로그램이 시작되면 PF7을 ADC7로 사용하기 위해 JTAG 기능을 비활성화함.
2. USART0를 9600bps, 데이터 8비트, 패리티 없음, 정지 비트 1개로 초기화함.
3. PF7을 입력으로 설정하고 ADC7 채널을 선택함.
4. ADC 기준전압을 AVCC로 설정하고 ADC 분주비를 128로 설정함.
5. PSD 센서의 출력이 안정화될 수 있도록 100ms 동안 대기함.
6. USART0을 통해 `PSD Distance Measurement Start` 문구를 출력함.
7. PSD 센서의 아날로그 출력값을 ADC로 16번 측정함.
8. 16개의 ADC 측정값을 평균 내어 노이즈를 감소시킴.
9. 평균 ADC 값을 0~5000mV 범위의 전압으로 변환함.
10. ADC 값이 거리표의 어느 두 기준값 사이에 있는지 확인함.
11. 두 기준값 사이의 거리를 선형 보간하여 실제 거리를 계산함.
12. ADC 값, 전압, 거리를 USART0을 통해 시리얼 터미널에 출력함.
13. 측정 범위를 벗어난 경우 `OUT OF RANGE`를 출력함.
14. ADC 값이 비정상적인 경우 `SENSOR ERROR`를 출력함.
15. 500ms마다 위 과정을 반복함.

### 정상 출력 예시

```text
PSD Distance Measurement Start
ADC: 266, Voltage: 1.300 V, Distance: 20.0 cm
ADC: 225, Voltage: 1.100 V, Distance: 25.0 cm
ADC: 194, Voltage: 0.948 V, Distance: 30.0 cm
```

측정된 ADC 값이 거리표에 저장된 기준값과 정확하게 일치하지 않는 경우에는 선형 보간을 통해 중간 거리값이 출력됨.

```text
ADC: 246, Voltage: 1.202 V, Distance: 22.4 cm
```

### 측정 범위 초과 출력 예시

```text
ADC: 70, Voltage: 0.342 V, Distance: OUT OF RANGE
```

거리표에 저장된 ADC 범위보다 작은 값이 측정된 경우 측정 가능한 거리 범위를 벗어난 것으로 판단함.

### 센서 오류 출력 예시

```text
ADC: 0, Voltage: 0.000 V, SENSOR ERROR
```

ADC 값이 0에 매우 가깝거나 1023에 매우 가까운 경우 센서 연결 또는 입력 상태가 비정상적인 것으로 판단함.

### 동작 사진 / 영상

|           동작 영상          |
| :----------------------: |
| https://drive.google.com/drive/folders/1o64ErgWdkI4Dj_jRctpaSXNeld2crE1i |

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)

본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool)  | 활용 영역    | 세부 사용 목적 및 내용                            |
| :---------- | :------- | :--------------------------------------- |
| **ChatGPT** | 개념 정리    | ADC, USART, JTAG 비활성화 및 PSD 센서의 동작 원리 참고 |
| **ChatGPT** | 코드 구성 참고 | ADC 평균값 계산, 전압 변환 및 거리표 구성 방법 참고         |
| **ChatGPT** | 계산 방법 참고 | 두 기준점 사이의 거리를 계산하는 선형 보간 방법 참고           |
| **ChatGPT** | 문서 작성 참고 | 프로그램의 동작 과정과 주요 레지스터 설정 내용 정리            |

### AI 활용 및 검증 원칙

1. AI가 제공한 ADC, USART 및 PSD 센서 관련 개념을 이해한 후 직접 코드를 작성함.
2. ATmega128 데이터시트를 통해 ADC와 USART 관련 레지스터의 설정값을 확인함.
3. PSD 센서 데이터시트의 출력 전압과 거리 관계를 확인하여 거리표를 구성함.
4. 센서와 물체 사이의 실제 거리를 직접 측정하여 계산된 거리값과 비교함.
5. 센서의 측정값이 흔들리는 현상을 확인하고 ADC 평균값 계산을 적용함.
6. 센서 연결 오류와 측정 범위 초과 상황에서 오류 문구가 정상적으로 출력되는지 확인함.
7. AI가 제시한 내용을 그대로 사용하지 않고 실제 하드웨어 환경과 코드 목적에 맞게 수정하고 검증함.
