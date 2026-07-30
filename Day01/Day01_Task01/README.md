# ATmega128 과제 및 프로젝트

> **광운대학교 로봇학부**  
> **작성자:** 김도훈
> **제출일:** 2026년 7월 30일

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
| **주요 부품** | ATmega128 개발보드, LED 센서, Tact Switch |

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
├─ Day01_Task01/
│   ├── main.c # 메인 제어 루프
└── README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)
### 레지스터 설정 코드
```c
	DDRA = 0xFF; // PINA 모두 출력으로 설정
	DDRE = 0x00; // PINE 모두 입력으로 설정
	PORTE = 0x03; // 내부 풀업 저항 활성화
	EICRA = 0xA0; // 10100000 2번 3번 인터럽트 falling edge로 설정
	EIMSK = 0x0C; // 00001100 2번 3번 인터럽트 활성화
```
###  while문 if문 실행 코드
```c
	while (1)
	{
		if (!(PINE & (1 << PINE4)) && !(PINE & (1 << PINE5))) // PE4와 PE5 눌렀을 때 실행
		{
			PORTA = 0x00; // active low 방식 00000000일 때 LED 출력
		}
		else if (!(PINE & (1 << PINE4))) // PE4 눌렀을 때 실행
		{
			PORTA = 0x0F;
		}
		else if (!(PINE & (1 << PINE5))) // PE5 눌렀을 때 실행
		{
			PORTA = 0xF0;
		}
		else
		{	
			// 조건문 조건 만족 안 될때 무한반복 코드
			PORTA = 0xFF; 
			_delay_ms(500);
			PORTA = 0x00;
			_delay_ms(500);
		}
	}
```
### 레지스터 설정 코드
```c
// 2번 인터럽트
ISR(INT2_vect){
	PORTA = 0xFF;
	for(int i = 0; i < 8; i++){
		PORTA =~ led[i];
		_delay_ms(100);
	}
}
// 3번 인터럽트
ISR(INT3_vect){
	PORTA = 0xFF;
	for(int i = 8; i >= 0; i--){
		PORTA =~ led[i];
		_delay_ms(100);
	}
}
```
---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오
1. 시스템 전원 인가 시 LED 1~8 번 모두 0.5초 마다 출력함
1. SW1 누를 시 우측 LED 4개 출력
2. SW2 누를 시 죄측 LED 4개 출력
3. SW1과 SW2 누를 시 LED 8개 출력
4. SW3 누를 시 LED 1~8번 순차 점등
5. SW4 누를 시 LED 1~8번 역순차 점등

### 동작 사진 / 영상

| 정면 동작 모습 |
| :---: | 
| ![Hardware Setup](https://drive.google.com/drive/folders/1FxS-o_upBDG0bxLEOcIdYU01ccSJoVhb)

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)
본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool) | 활용 영역 | 세부 사용 목적 및 내용 |
| :--- | :--- | :--- |
| **ChatGPT** | 개념 정리 | - 각종 레지스터 개념과 사용법 참고, 내부 풀업 저항 활성화법 참고 |

### AI 활용 및 검증 원칙
1. **본인 검증: ** AI가 생성한 개념 및 사용법 이해 후 코드 작성(코드 작성 시 참고하지 않음)
