# IR_Sensor_TASK

> **광운대학교 로봇학부**
> **작성자:** 김도훈
> **제출일:** 2026년 8월 10일

---

## 1. 개요 (Overview)

본 과제는 ATmega128 마이크로컨트롤러의 ADC를 활용하여 6개의 IR 센서값을 측정하고, 이동평균필터(MAF)를 적용하여 센서값을 안정화하는 프로그램을 구현하는 것을 목적으로 한다.

각 IR 센서에서 측정된 값을 센서별 최솟값과 최댓값을 이용하여 0.00 ~ 1.00 범위로 정규화하고, 정규화 값이 0.8 이상인 경우 해당 번호의 LED를 점등하도록 구현하였다.

또한 USART를 통해 원본 ADC 값, 필터값, 최솟값, 최댓값, 정규화 값을 확인하고 I2C LCD를 이용하여 6개 센서의 정규화 값을 실시간으로 확인할 수 있도록 구현하였다.

### 핵심 목표

- ATmega128의 ADC를 이용하여 6개의 IR 센서값을 측정한다.
- 이동평균필터(MAF)를 적용하여 센서값의 노이즈를 감소시킨다.
- 각 센서의 최솟값과 최댓값을 이용하여 센서값을 정규화한다.
- 정규화 값이 0.8 이상일 경우 해당 번호의 LED를 점등한다.
- USART를 이용하여 센서의 원본값, 필터값, MIN, MAX, 정규화 값을 출력한다.
- I2C LCD에 6개 IR 센서의 정규화 값을 출력한다.

---

## 2. 개발 환경 (Environment)

| 항목 | 내용 |
| :--- | :--- |
| **MCU** | ATmega128A (16MHz External Crystal) |
| **IDE / Compiler** | Microchip Studio 7.0 / Microchip AVR GCC |
| **Flasher Tool** | USBISP / STK500 |
| **언어** | C Language |
| **주요 부품** | ATmega128 개발보드, IR 센서 6개, LED 6개, I2C LCD |

---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]                 [Target Component]

PF1 / ADC1  ------------->  IR Sensor 1
PF2 / ADC2  ------------->  IR Sensor 2
PF3 / ADC3  ------------->  IR Sensor 3
PF4 / ADC4  ------------->  IR Sensor 4
PF5 / ADC5  ------------->  IR Sensor 5
PF6 / ADC6  ------------->  IR Sensor 6

PA0  -------------------->  LED 1
PA1  -------------------->  LED 2
PA2  -------------------->  LED 3
PA3  -------------------->  LED 4
PA4  -------------------->  LED 5
PA5  -------------------->  LED 6

TWI  -------------------->  I2C LCD
USART0 ------------------>  PC Serial Terminal
```

### 주요 회로 특징

- **전원:** 5V DC 안정화 전원 공급
- **IR 센서:** PF1 ~ PF6의 ADC 채널을 이용하여 센서값 측정
- **LED:** PA0 ~ PA5를 출력으로 설정하여 센서 상태 표시
- **LCD:** I2C 통신을 이용하여 정규화된 센서값 출력
- **USART:** PC 터미널을 통해 센서값 확인
- **주의사항:** PF4 ~ PF6은 JTAG 기능과 핀을 공유하기 때문에 ADC로 사용하기 위해 JTAG 기능을 비활성화해야 한다.

---

## 4. 프로젝트 구조 (Directory Structure)

> 구현부(.c), 선언부(.h)만 구조에 표기함.

```text
├─ IR_Sensor_Task/
│   ├── main.c          # IR 센서 및 전체 제어
│   ├── i2c_lcd.c       # I2C LCD 제어
│   ├── i2c_lcd.h       # I2C LCD 함수 선언
└── README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### ADC 및 JTAG 설정 코드

```c
void ADC_Init(void)
{
    DDRF = 0x00;                               // PF 입력
    PORTF = 0x00;                              // Pull-up OFF
    ADMUX = (1 << REFS0);                     // 기준전압 AVCC
    ADCSRA = (1 << ADEN) | (1 << ADPS2) |
             (1 << ADPS1) | (1 << ADPS0);     // ADC 활성화, 분주비 128
}

void JTAG_Disable(void)
{
    MCUCSR |= (1 << JTD);
    MCUCSR |= (1 << JTD);                     // PF4~PF6 ADC 사용
}
```

ADC 기준 전압은 AVCC를 사용하고 분주비를 128로 설정하였다.

MCU의 클럭이 16MHz이므로 ADC 클럭은 다음과 같다.

```text
16MHz / 128 = 125kHz
```

6개의 IR 센서는 ADC1 ~ ADC6을 사용한다.

---

### ADC 값 측정

```c
uint16_t ADC_Read(uint8_t ch)
{
    ADMUX = (ADMUX & 0xE0) | ch;              // ADC 채널 선택
    ADCSRA |= (1 << ADSC);                    // ADC 변환 시작

    while (ADCSRA & (1 << ADSC));

    return ADC;
}
```

ADC는 10비트이므로 측정된 센서값은 다음 범위를 가진다.

```text
0 ~ 1023
```

6개의 IR 센서는 반복문을 이용하여 순서대로 측정한다.

```c
raw[i] = ADC_Read(i + 1);                     // ADC1 ~ ADC6 측정
```

---

### 이동평균필터 (MAF)

IR 센서에서 측정되는 값은 주변 환경이나 노이즈에 의해 계속 조금씩 변할 수 있다.

이를 줄이기 위해 최근 5개의 센서값을 저장한 후 평균을 계산하는 이동평균필터를 적용하였다.

```c
for (j = 0; j < MAF; j++)
    sum += maf_buf[i][j];

filter[i] = sum / MAF;                        // 최근 5개 값의 평균
```

이동평균필터를 이용하여 순간적으로 발생하는 센서값의 변화를 감소시키고 보다 안정적인 값을 얻을 수 있다.

---

### MIN / MAX 측정

각 센서마다 측정되는 값의 범위가 다를 수 있기 때문에 센서별 최솟값과 최댓값을 저장한다.

```c
if (filter[i] < min[i])
    min[i] = filter[i];

if (filter[i] > max[i])
    max[i] = filter[i];
```

센서값이 기존 최솟값보다 작으면 MIN을 변경하고 기존 최댓값보다 크면 MAX를 변경한다.

---

### 정규화 (Normalization)

센서마다 서로 다른 ADC 측정 범위를 동일한 기준으로 사용하기 위해 센서값을 0.00 ~ 1.00 범위로 정규화하였다.

정규화 식은 다음과 같다.

```text
              Filter - Min
Norm = -------------------------
                Max - Min
```

프로그램에서는 실수 연산을 사용하지 않고 0 ~ 100 범위의 정수로 변환하여 계산하였다.

```c
norm[i] = (uint32_t)(filter[i] - min[i]) * 100 /
          (max[i] - min[i]);                  // 0~100 정규화
```

각 값은 다음과 같은 의미를 가진다.

```text
0   → 0.00
20  → 0.20
50  → 0.50
80  → 0.80
100 → 1.00
```

---

### LED 제어

6개의 LED는 PA0 ~ PA5에 연결하였다.

IR 센서의 정규화 값이 0.8 이상이면 해당 번호의 LED를 점등하고 0.8 미만이면 LED를 소등한다.

```c
for (i = 0; i < 6; i++)
{
    if (norm[i] >= 80)
        PORTA |= (1 << i);                    // 0.8 이상 LED ON
    else
        PORTA &= ~(1 << i);                   // 0.8 미만 LED OFF
}
```

예를 들어 IR2의 정규화 값이 0.91이면 LED2가 켜지게 된다.

---

### USART 설정 및 출력

USART0은 9600bps로 설정하여 PC 터미널에서 IR 센서값을 확인할 수 있도록 하였다.

```c
void UART0_Init(void)
{
    UBRR0H = 0;
    UBRR0L = 103;                             // 9600bps
    UCSR0B = (1 << TXEN0);                    // 송신 활성화
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);  // 8bit
}
```

USART에는 각 센서의 원본값, 이동평균필터 값, MIN, MAX, 정규화 값을 출력한다.

```text
      original / filter / min / max / norm

IR1 : 243 / 250 / 233 / 921 / 0.02
IR2 : 945 / 943 / 240 / 964 / 0.97
IR3 : 874 / 785 / 221 / 870 / 0.87
IR4 : 668 / 662 / 254 / 896 / 0.64
IR5 : 224 / 226 / 221 / 932 / 0.00
IR6 : 784 / 781 / 333 / 823 / 0.91
```

이를 통해 센서의 원본값과 필터 및 정규화 과정을 확인할 수 있다.

---

### I2C LCD 출력

I2C LCD에는 6개의 IR 센서 정규화 값을 출력한다.

첫 번째 줄에는 IR1 ~ IR3을 출력하고 두 번째 줄에는 IR4 ~ IR6을 출력한다.

```c
i2c_lcd_goto_xy(0, 0);                       // IR1~IR3
i2c_lcd_goto_xy(1, 0);                       // IR4~IR6
```

LCD 출력 예시는 다음과 같다.

```text
1:0.12 2:0.85 3:0.43
4:0.91 5:0.05 6:0.78
```

---

### main 반복문

```c
while (1)
{
    IR_Read();                               // ADC + MAF + MIN/MAX + 정규화
    LED_Control();                           // 0.8 이상 LED ON
    UART_Print();                            // USART 출력
    LCD_Print();                             // LCD 출력

    _delay_ms(200);
}
```

프로그램은 IR 센서값을 계속 측정하면서 필터, 정규화, LED 제어, USART 출력 및 LCD 출력을 반복한다.

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오

1. 시스템에 전원을 인가하면 ADC와 USART, I2C LCD 및 LED를 초기화한다.
2. PF1 ~ PF6에 연결된 6개의 IR 센서값을 ADC를 이용하여 측정한다.
3. 각 IR 센서의 최근 5개 측정값에 이동평균필터를 적용한다.
4. 각 센서의 필터값을 이용하여 MIN과 MAX를 저장한다.
5. MIN과 MAX를 이용하여 센서값을 0.00 ~ 1.00 범위로 정규화한다.
6. 정규화된 값이 0.8 이상이면 해당 번호의 LED가 점등된다.
7. USART를 통해 Original, Filter, MIN, MAX, Norm 값을 출력한다.
8. LCD 첫 번째 줄에는 IR1 ~ IR3, 두 번째 줄에는 IR4 ~ IR6의 정규화 값을 출력한다.
9. 위 과정을 0.2초 간격으로 반복한다.

### 전체 동작 순서

```text
IR 센서 ADC 측정
        ↓
이동평균필터(MAF)
        ↓
MIN / MAX 갱신
        ↓
정규화
        ↓
Norm >= 0.8
        ↓
LED ON / OFF
        ↓
USART 출력
        ↓
LCD 출력
```

### 동작 사진 / 영상

| 정면 동작 모습 |
| :---: |
| https://drive.google.com/drive/folders/1x8ByFjWSCfdfoOzbYv4eRb07Ov4UYSTp |

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)

본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool) | 활용 영역 | 세부 사용 목적 및 내용 |
| :--- | :--- | :--- |
| **ChatGPT** | 개념 정리 및 코드 검토 | ADC, 이동평균필터, 정규화, USART 및 I2C LCD 동작 방법 참고 |

### AI 활용 및 검증 원칙

1. **본인 검증:** AI가 제공한 개념 및 코드의 동작 원리를 이해한 후 실제 회로에서 동작 여부를 확인하였다.
2. **직접 검증:** USART와 LCD에 출력되는 센서값을 확인하고 IR 센서 및 LED의 실제 동작과 비교하여 프로그램을 검증하였다.