# ATmega128 과제 및 프로젝트 템플릿

> **광운대학교 로봇학부**  
> **작성자:** 김도훈
> **제출일:** 2026년 7월 31일

---

## 1. 개요 (Overview)
본 과제는 ATmega128 마이크로컨트롤러를 활용하여 주요 주변장치(Peripherals)를 제어하고 센서 데이터를 수신/처리하는 시스템을 구현하는 것을 목표로 함.

### 핵심 목표
* ATmega128 레지스터 설정을 통한 주변장치 제어
* 센서 및 외부 모듈과의 통신 (USART / SPI / I2C 등) 및 데이터 처리
* 타이머/카운터를 활용한 PWM 출력 및 인터럽트 제어

---

## 2. 개발 환경 (Environment)

| 항목 | 내용 |
| :--- | :--- |
| **MCU** | ATmega128A (16MHz External Crystal) |
| **IDE / Compiler** | Microchip Studio 7.0 / Microchip AVR GCC |
| **Flasher Tool** | USBISP / STK500 |
| **언어** | C Language |
| **주요 부품** | ATmega128 개발보드, LED 센서, Tact Switch, LCD, 가변저항 |

---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]                 [Target Component]
 PORTA (PA0 ~ PA7)   ----->   8-Bit LED
 PE4 / PE5 / PD4 / PD5 -----> Tact Switch
```

### 주요 회로 특징
* **전원:** 5V DC 안정화 전원 공급
* **주의사항:** ISP 다운로드 시 SPI 핀 타겟 전원 및 리셋 회로 간섭 주의

---

## 4. 프로젝트 구조 (Directory Structure)
> 구현부(.c), 선언부(.h)만 구조에 표기함.
```text
├─ Day02_Task02/
│   ├── main.c # 메인 제어 루프
│   ├── i2c_lcd.c # 헤더 파일 소스 코드
│   ├── i2c_lcd.h # 헤더 파일 
└── README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)
### ADC 설정 코드
```c
void adc_init(void)
{
	ADMUX = (1 << REFS0); // 기준 전압 AVCC, ADC 결과 오른쪽 정렬
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // ADC 활성화, 분주비 128
}
```
###  ADC 진행 코드
```c
uint16_t adc_read(uint8_t channel)
{
	ADMUX = (ADMUX & 0xE0) | (channel & 0x1F); // ADC 채널 선택
	ADCSRA |= (1 << ADSC); // ADC 변환 시작
	
	while (ADCSRA & (1 << ADSC)); // ADC 변환이 끝날 때까지 대기
	
	return ADC; // ADC 변환값 반환
}
```
### LCD 출력 코드
```c
	snprintf(lcd, sizeof(lcd), "%4u %u.%02uV", adc_value, volts, frac); // 출력 문자열 만들기
	i2c_lcd_goto_xy(1, 0); // LCD 두 번째 줄로 이동
	i2c_lcd_string(lcd); // ADC값과 전압 출력
```
---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오
1. LCD에 이름 이니셜 출력
1. 가변 저항을 변화 시키면 그 값에 따라 LED가 움직인다.
1. 변환되고 변화되는 ADC값이 LCD에 출력
2. ADC 값을 계산해서 현재 가변저항의 전압 값을 LCD에 표시


### 동작 사진 / 영상

| 정면 동작 모습 |
| :---: | 
| ![Hardware Setup](https://drive.google.com/drive/folders/17oH6pEp4PQMTKcmM4A9Slx8wekH6DrxP)

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)
본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool) | 활용 영역 | 세부 사용 목적 및 내용 |
| :--- | :--- | :--- |
| **ChatGPT** | 개념 정리 | - 각종 레지스터 개념과 사용법 참고, 헤더 파일에 들어가야 할 함수, 구성 참고  |

### AI 활용 및 검증 원칙
1. **본인 검증: ** AI가 생성한 개념 및 사용법 이해 후 코드 작성(코드 작성 시 참고하지 않음)
