# Day04_Task02

> **광운대학교 로봇학부**
> **작성자:** 김도훈
> **제출일:** 2026년 8월 2일

---

## 1. 개요 (Overview)

본 과제는 ATmega128 마이크로컨트롤러의 ADC, Timer/Counter, 스위치 및 I2C LCD를 활용하여 날짜와 시간을 설정하고 출력하는 디지털 시계를 구현하는 것을 목표로 함.

가변저항의 아날로그 입력값을 ADC로 측정하여 연도, 월, 일, 시, 분, 초, 밀리초 값으로 변환함. 첫 번째 스위치를 눌러 각 설정값을 순서대로 확정하고, 두 번째 스위치를 누르면 설정한 시간부터 시계가 동작하도록 구현함.

### 핵심 목표

* ADC를 이용한 가변저항 입력값 측정
* ADC 값을 날짜 및 시간 범위로 변환
* 스위치를 이용한 날짜 및 시간 설정
* Timer0 비교 일치 인터럽트를 이용한 1ms 단위 시간 측정
* 윤년 및 월별 마지막 날짜 계산
* I2C LCD를 이용한 날짜 및 시간 출력

---

## 2. 개발 환경 (Environment)

| 항목                 | 내용                                                 |
| :----------------- | :------------------------------------------------- |
| **MCU**            | ATmega128A (16MHz External Crystal)                |
| **IDE / Compiler** | Microchip Studio 7.0 / Microchip AVR GCC           |
| **Flasher Tool**   | USBISP / STK500                                    |
| **언어**             | C Language                                         |
| **주요 부품**          | ATmega128 개발보드, Tact Switch 2개, 16×2 I2C LCD, 가변저항 |

---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]                  [Target Component]

PF0 / ADC0            -----> 가변저항 출력 단자
PE4                   -----> SW1 설정값 확정 스위치
PE5                   -----> SW2 시계 시작 스위치
I2C 통신 핀           -----> 16×2 I2C LCD
```

### 주요 회로 특징

* **전원:** 5V DC 안정화 전원 공급
* **가변저항:** 양 끝 단자를 VCC와 GND에 연결하고 가운데 출력 단자를 PF0에 연결
* **스위치:** PE4와 PE5에 연결하고 ATmega128의 내부 풀업 저항 사용
* **ADC 입력:** PF0의 내부 풀업 저항은 비활성화
* **LCD:** I2C 통신을 이용하여 날짜와 시간 출력
* **주의사항:** ISP 다운로드 시 SPI 핀, 타겟 전원 및 리셋 회로 간섭 주의

---

## 4. 프로젝트 구조 (Directory Structure)

> 구현부(.c), 선언부(.h)만 구조에 표기함.

```text
├─ Digital_Clock/
│   ├── main.c       # ADC, 스위치, Timer0 및 메인 제어 코드
│   ├── i2c_lcd.c    # I2C LCD 제어 함수 구현
│   ├── i2c_lcd.h    # I2C LCD 함수 선언
└── README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### 날짜 및 시간 구조체

```c
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
```

연도, 월, 일, 시, 분, 초, 밀리초 값을 하나의 구조체로 관리함. 현재 시간은 Timer0 인터럽트에서도 변경되므로 `volatile`로 선언함.

```c
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
```

초기값은 `00년 01월 01일 00시 00분 00초 000밀리초`임.

### 설정 항목 열거형

```c
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
```

현재 어떤 항목을 설정하고 있는지 구분하기 위해 열거형을 사용함.

설정 순서는 다음과 같음.

```text
연도 → 월 → 일 → 시 → 분 → 초 → 밀리초
```

### 스위치 확인 함수

```c
uint8_t Switch_Pressed(uint8_t pin)
{
	if (!(PINE & (1 << pin)))
	{
		_delay_ms(20);

		if (!(PINE & (1 << pin)))
		{
			while (!(PINE & (1 << pin)));

			_delay_ms(20);

			return 1;
		}
	}

	return 0;
}
```

PE4 또는 PE5에 연결된 스위치가 한 번 눌렸는지 확인함. 스위치를 누를 때와 뗄 때 각각 20ms의 지연을 주어 채터링을 방지함.

스위치에는 내부 풀업 저항을 사용하므로 평상시에는 HIGH이고 스위치를 누르면 LOW가 입력됨.

### ADC 초기화

```c
void ADC_Init(void)
{
	DDRF &= ~(1 << DDF0);
	PORTF &= ~(1 << PF0);

	ADMUX = (1 << REFS0);

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

PF0을 입력으로 설정하고 내부 풀업 저항을 비활성화함.

`ADMUX` 레지스터의 `REFS0` 비트를 1로 설정하여 AVCC를 ADC 기준전압으로 사용함. 채널 선택 비트인 `MUX4~MUX0`은 모두 0이므로 ADC0 채널이 선택됨.

`ADCSRA` 레지스터의 `ADEN` 비트를 설정하여 ADC를 활성화하고, `ADPS2~ADPS0`을 모두 1로 설정하여 분주비 128을 사용함.

```text
ADC 클럭 = 16MHz ÷ 128 = 125kHz
```

ADC 초기화 직후의 첫 번째 변환값은 불안정할 수 있으므로 첫 번째 측정 결과는 사용하지 않고 버림.

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

	return (uint16_t)(sum / ADC_SAMPLE_COUNT);
}
```

ADC 값을 8번 측정한 후 평균값을 계산함. 이를 통해 가변저항 입력값에 포함된 작은 노이즈와 값의 흔들림을 줄임.

### ADC 값 범위 변환

```c
uint16_t ADC_Map(
	uint16_t adc_value,
	uint16_t minimum,
	uint16_t maximum)
{
	uint32_t result;

	result =
		(uint32_t)adc_value *
		(maximum - minimum);

	result = (result + 511UL) / 1023UL;

	return minimum + (uint16_t)result;
}
```

ADC의 측정 범위인 0~1023을 각 날짜와 시간 항목에 필요한 범위로 변환함.

| 설정 항목 |      변환 범위     |
| :---- | :------------: |
| 연도    |      0~99      |
| 월     |      1~12      |
| 일     | 1~해당 월의 마지막 날짜 |
| 시     |      0~23      |
| 분     |      0~59      |
| 초     |      0~59      |
| 밀리초   |      0~999     |

### 윤년 및 월별 날짜 계산

```c
uint8_t Is_LeapYear(uint8_t year)
{
	if ((year % 4) == 0)
	{
		return 1;
	}

	return 0;
}
```

연도 값은 2000년부터 2099년까지를 의미함. 이 범위에서는 연도가 4의 배수이면 윤년으로 판단함.

```c
uint8_t Days_InMonth(uint8_t year, uint8_t month)
{
	if (month == 2)
	{
		if (Is_LeapYear(year))
		{
			return 29;
		}

		return 28;
	}

	if (month == 4 ||
		month == 6 ||
		month == 9 ||
		month == 11)
	{
		return 30;
	}

	return 31;
}
```

2월은 윤년이면 29일, 평년이면 28일로 계산함. 4월, 6월, 9월, 11월은 30일까지이며 나머지 월은 31일까지로 계산함.

### Timer0 초기화 및 시작

```c
void Timer0_Init(void)
{
	TCCR0 = 0x00;
	TCNT0 = 0;
	OCR0 = 249;

	TCCR0 = (1 << WGM01);

	TIMSK |= (1 << OCIE0);
}
```

Timer0를 CTC 모드로 설정하고 비교 일치값을 249로 설정함. `OCIE0` 비트를 설정하여 Timer0 비교 일치 인터럽트를 활성화함.

초기화 단계에서는 클럭 선택 비트를 설정하지 않기 때문에 Timer0는 정지 상태로 유지됨.

```c
void Timer0_Start(void)
{
	TCNT0 = 0;
	TIFR = (1 << OCF0);

	TCCR0 =
		(1 << WGM01) |
		(1 << CS01) |
		(1 << CS00);

	sei();
}
```

Timer0 시작 시 `CS01`과 `CS00`을 설정하여 분주비 64를 사용함.

```text
16MHz ÷ 64 = 250kHz

1카운트 시간 = 1 ÷ 250,000
              = 4µs

4µs × 250카운트 = 1ms
```

따라서 Timer0 비교 일치 인터럽트는 1ms마다 발생함.

### Timer0 비교 일치 인터럽트

```c
ISR(TIMER0_COMP_vect)
{
	clock_time.millisecond++;

	if (clock_time.millisecond >= 1000)
	{
		clock_time.millisecond = 0;
		clock_time.second++;

		if (clock_time.second >= 60)
		{
			clock_time.second = 0;
			clock_time.minute++;

			if (clock_time.minute >= 60)
			{
				clock_time.minute = 0;
				clock_time.hour++;

				if (clock_time.hour >= 24)
				{
					clock_time.hour = 0;
					clock_time.day++;

					if (clock_time.day >
						Days_InMonth(
							clock_time.year,
							clock_time.month
						))
					{
						clock_time.day = 1;
						clock_time.month++;

						if (clock_time.month > 12)
						{
							clock_time.month = 1;
							clock_time.year++;

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
```

Timer0 인터럽트가 발생할 때마다 밀리초 값을 1씩 증가시킴.

```text
1000밀리초 → 1초
60초       → 1분
60분       → 1시간
24시간     → 1일
월의 마지막 날짜 초과 → 다음 달
12월 초과  → 다음 연도
99년 초과  → 00년
```

### LCD 출력

```c
void Display_DateTime(const DateTime *time)
{
	char line1[17];
	char line2[17];

	snprintf(
		line1,
		sizeof(line1),
		"%02u-%02u-%02u",
		(unsigned int)time->year,
		(unsigned int)time->month,
		(unsigned int)time->day
	);

	snprintf(
		line2,
		sizeof(line2),
		"%02u:%02u:%02u.%03u",
		(unsigned int)time->hour,
		(unsigned int)time->minute,
		(unsigned int)time->second,
		(unsigned int)time->millisecond
	);

	LCD_WriteLine(0, line1);
	LCD_WriteLine(1, line2);
}
```

LCD의 첫 번째 줄에는 연도, 월, 일을 출력하고 두 번째 줄에는 시, 분, 초, 밀리초를 출력함.

```text
26-07-31
14:25:37.125
```

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오

1. 프로그램이 시작되면 LCD, 스위치, ADC 및 Timer0가 초기화됨.
2. 가변저항을 돌려 연도 값을 00부터 99까지 설정함.
3. 첫 번째 스위치인 SW1을 누르면 현재 연도 값이 확정되고 월 설정으로 이동함.
4. 가변저항을 돌려 월 값을 1부터 12까지 설정함.
5. SW1을 누르면 월 값이 확정되고 일 설정으로 이동함.
6. 일은 현재 설정된 연도와 월에 맞는 범위 안에서 설정됨.
7. 같은 방법으로 시, 분, 초, 밀리초를 순서대로 설정함.
8. 각 항목을 확정할 때마다 SW1을 누름.
9. 모든 설정이 완료되면 두 번째 스위치인 SW2를 누르기 전까지 시간이 정지된 상태로 유지됨.
10. SW2를 누르면 Timer0가 시작되고 설정한 시간부터 시계가 동작함.
11. LCD 첫 번째 줄에는 날짜가 출력되고 두 번째 줄에는 시간이 출력됨.
12. 1ms마다 Timer0 인터럽트가 발생하여 밀리초 값이 증가함.
13. 자정을 지나면 날짜가 증가하고, 월의 마지막 날짜를 지나면 다음 달로 변경됨.
14. 12월의 마지막 날짜를 지나면 다음 연도로 변경됨.

### 동작 결과 예시

```text
26-07-31
23:59:59.999
```

1ms가 지나면 다음과 같이 변경됨.

```text
26-08-01
00:00:00.000
```

윤년인 2024년의 경우 다음과 같이 동작함.

```text
24-02-28
23:59:59.999
```

1ms가 지나면 다음과 같이 변경됨.

```text
24-02-29
00:00:00.000
```

평년인 2025년의 경우 2월 28일 다음 날은 3월 1일로 변경됨.

### 동작 사진 / 영상

|    동작 영상   |
| :-----------: |
| https://drive.google.com/drive/folders/1o64ErgWdkI4Dj_jRctpaSXNeld2crE1i |

## 7. AI 툴 활용 명시 (AI Tools Declaration)

본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool)  | 활용 영역    | 세부 사용 목적 및 내용                            |
| :---------- | :------- | :--------------------------------------- |
| **ChatGPT** | 개념 정리    | ADC, Timer0, CTC 모드, 분주비 및 인터럽트 동작 원리 참고 |
| **ChatGPT** | 코드 구성 참고 | ADC 입력값 범위 변환, 윤년 판단 및 월별 날짜 계산 방법 참고    |
| **ChatGPT** | 문서 작성 참고 | 프로그램의 동작 과정과 주요 레지스터 설정 내용 정리            |

### AI 활용 및 검증 원칙

1. AI가 생성한 개념과 사용법을 이해한 후 직접 코드를 작성함.
2. ADC, Timer0 및 인터럽트 관련 레지스터 설정값을 직접 확인함.
3. 가변저항, 스위치 및 LCD를 실제 회로에 연결하여 동작을 검증함.
4. 윤년과 월별 날짜 변경 기능이 정상적으로 동작하는지 직접 확인함.
5. AI가 제시한 내용을 그대로 사용하지 않고 코드의 목적에 맞게 수정하고 검증함.
