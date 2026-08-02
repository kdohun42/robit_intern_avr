# Day02_Task02

> **광운대학교 로봇학부**  
> **작성자:** 김도훈
> **제출일:** 2026년 7월 31일

---

## 1. 개요 (Overview)
본 과제는 ATmega128과 4개의 스위치를 활용하여 덧셈, 뺄셈, 곱셈, 나눗셈이 가능한 간단한 계산기를 구현하는 것을 목적으로 한다. 각 스위치를 이용해 피연산자 A와 B의 값을 변경하고 사칙연산자를 선택한 뒤, 계산 결과를 LCD에 출력한다. 

### 핵심 목표
* 스위치 입력을 이용하여 피연산자 A와 B의 값을 1씩 증가시킨다.
* 스위치를 누를 때마다 덧셈, 뺄셈, 곱셈, 나눗셈 연산자를 순서대로 변경한다. 
* 선택한 연산을 수행하고 계산식과 결과를 LCD에 출력한다.

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
### 스위치 확인 함수
```c
// E 포트 스위치가 한 번 눌렸는지 확인
uint8_t switch_e_pressed(uint8_t pin)
{
	if (!(PINE & (1 << pin))) // 스위치가 눌렸는지 확인
	{
		_delay_ms(20); // 채터링 방지
		
		if (!(PINE & (1 << pin))) // 스위치 상태 다시 확인
		{
			while (!(PINE & (1 << pin))); // 스위치를 뗄 때까지 대기
			_delay_ms(20); // 스위치를 뗄 때 발생하는 채터링 방지
			
			return 1; // 스위치가 눌렸음을 반환
		}
	}
	
	return 0; // 스위치가 눌리지 않았음을 반환
}
```
```c
// D 포트 스위치가 한 번 눌렸는지 확인
uint8_t switch_d_pressed(uint8_t pin)
{
	if (!(PIND & (1 << pin))) // 스위치가 눌렸는지 확인
	{
		_delay_ms(20); // 채터링 방지
		
		if (!(PIND & (1 << pin))) // 스위치 상태 다시 확인
		{
			while (!(PIND & (1 << pin))); // 스위치를 뗄 때까지 대기
			_delay_ms(20); // 스위치를 뗄 때 발생하는 채터링 방지
			
			return 1; // 스위치가 눌렸음을 반환
		}
	}
	
	return 0; // 스위치가 눌리지 않았음을 반환
}
---
```
## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오
1. 첫 번쨰 스위치를 누를때마다 A의 값이 오름
1. 두번째 스위치를 누를 때마다 산술 연산자가 바뀐다. (순서는 ‘+’ ‘-‘ ‘*’ ‘/’ 순)
3. 세번째 스위치를 누를 때마다 B의 값이 1씩 오른다. (초기값 B = 1)
4. 네번째 스위치를 누르면 연산을 하여 값을 LCD에 띄운다.


### 동작 사진 / 영상

| 정면 동작 모습 |
https://drive.google.com/drive/folders/17oH6pEp4PQMTKcmM4A9Slx8wekH6DrxP

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)
본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool) | 활용 영역 | 세부 사용 목적 및 내용 |
| :--- | :--- | :--- |
| **ChatGPT** | 개념 정리 | - 각종 레지스터 개념과 사용법 참고, 헤더 파일에 들어가야 할 함수 구성및 구현 참고, 연산자 사용법 등  |

### AI 활용 및 검증 원칙
1. **본인 검증: ** AI가 생성한 개념 및 사용법 이해 후 코드 작성(코드 작성 연산자는 참고)
