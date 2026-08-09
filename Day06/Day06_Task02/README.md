# Motor_Control_REPORT

> **광운대학교 로봇학부**
> **작성자:** 김도훈
> **제출일:** 2026년 8월 10일

---

## 1. 개요 (Overview)

본 과제는 ATmega128 마이크로컨트롤러의 PORTB를 이용하여 2개의 DC 모터를 제어하는 프로그램을 구현하는 것을 목적으로 한다.

모터 드라이버의 입력 핀과 Enable 핀을 ATmega128의 PORTB에 연결하고, 각 입력 핀의 HIGH/LOW 상태를 설정하여 두 모터가 같은 방향으로 회전하도록 구현하였다.

### 핵심 목표

- ATmega128의 PORTB를 출력으로 설정한다.
- 모터 드라이버의 ENA, ENB를 활성화한다.
- 모터 1의 방향을 제어한다.
- 모터 2의 방향을 제어한다.
- 2개의 DC 모터를 동시에 동작시킨다.

---

## 2. 개발 환경 (Environment)

| 항목 | 내용 |
| :--- | :--- |
| **MCU** | ATmega128A (16MHz External Crystal) |
| **IDE / Compiler** | Microchip Studio 7.0 / Microchip AVR GCC |
| **Flasher Tool** | USBISP / STK500 |
| **언어** | C Language |
| **주요 부품** | ATmega128 개발보드, 모터 드라이버, DC Motor 2개 |

---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]              [Motor Driver]

PB0  ----------------->  Motor 1 Input 1
PB1  ----------------->  Motor 1 Input 2

PB2  ----------------->  Motor 2 Input 1
PB3  ----------------->  Motor 2 Input 2

PB5  ----------------->  ENA
PB6  ----------------->  ENB
```

### 주요 회로 특징

- PB0, PB1을 이용하여 모터 1의 회전 방향을 결정한다.
- PB2, PB3을 이용하여 모터 2의 회전 방향을 결정한다.
- PB5의 ENA를 활성화하여 모터 1 출력을 허용한다.
- PB6의 ENB를 활성화하여 모터 2 출력을 허용한다.
- ATmega128과 모터 드라이버의 GND는 공통으로 연결하여 사용한다.

---

## 4. 프로젝트 구조 (Directory Structure)

> 구현부(.c)만 구조에 표기함.

```text
├─ Motor_Control/
│   ├── main.c          # DC 모터 제어
└── README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### PORTB 출력 설정

```c
DDRB = 0x6F;   // PB0~PB3, PB5, PB6 출력
```

`DDRB`는 PORTB 핀의 입력과 출력을 설정하는 레지스터이다.

`0x6F`를 2진수로 나타내면 다음과 같다.

```text
0x6F = 0110 1111
```

따라서 다음 핀이 출력으로 설정된다.

```text
PB0 → 출력
PB1 → 출력
PB2 → 출력
PB3 → 출력
PB5 → 출력
PB6 → 출력
```

---

### PORTB 초기화

```c
PORTB = 0x00;   // 모든 출력 LOW
```

프로그램 시작 시 PORTB의 모든 출력값을 LOW로 초기화하였다.

이를 통해 모터 제어 핀이 초기 상태에서 임의의 값을 가지는 것을 방지한다.

---

### ENA, ENB 활성화

```c
PORTB |= (1 << PB5) | (1 << PB6);   // ENA, ENB 활성화
```

PB5와 PB6을 HIGH로 설정하여 두 모터의 Enable 입력을 활성화한다.

```text
PB5 = 1 → ENA 활성화
PB6 = 1 → ENB 활성화
```

Enable 핀이 활성화되어 있어야 모터 드라이버의 출력이 모터로 전달된다.

---

### 모터 1 제어

```c
PORTB |=  (1 << PB0);
PORTB &= ~(1 << PB1);
```

모터 1의 두 입력을 다음과 같이 설정하였다.

```text
PB0 = HIGH
PB1 = LOW
```

두 입력 핀의 상태를 서로 다르게 설정함으로써 모터 1을 한 방향으로 회전시킨다.

회전 방향을 반대로 변경하려면 다음과 같이 설정할 수 있다.

```c
PORTB &= ~(1 << PB0);
PORTB |=  (1 << PB1);
```

---

### 모터 2 제어

```c
PORTB |=  (1 << PB2);
PORTB &= ~(1 << PB3);
```

모터 2도 모터 1과 동일한 방식으로 제어하였다.

```text
PB2 = HIGH
PB3 = LOW
```

이를 통해 모터 2를 한 방향으로 회전시킨다.

---

### Delay 설정

```c
_delay_ms(3000);
```

두 모터의 방향을 설정한 이후 3000ms, 즉 3초 동안 대기한다.

현재 코드에서는 3초 후에도 모터의 출력 상태를 변경하지 않고 같은 명령을 반복하기 때문에 두 모터는 계속 같은 방향으로 회전한다.

---

### 전체 코드

```c
#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
    DDRB = 0x6F;   // 출력 설정

    PORTB = 0x00;  // 전부 LOW

    PORTB |= (1 << PB5) | (1 << PB6);  // ENA, ENB 활성화

    while (1)
    {
        PORTB |=  (1 << PB0);          // Motor 1 IN1 HIGH
        PORTB &= ~(1 << PB1);          // Motor 1 IN2 LOW

        PORTB |=  (1 << PB2);          // Motor 2 IN1 HIGH
        PORTB &= ~(1 << PB3);          // Motor 2 IN2 LOW

        _delay_ms(3000);               // 3초 대기
    }
}
```

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오

1. 시스템에 전원을 인가한다.
2. PB0 ~ PB3, PB5, PB6을 출력 핀으로 설정한다.
3. PORTB의 모든 출력을 LOW로 초기화한다.
4. PB5와 PB6을 HIGH로 설정하여 ENA와 ENB를 활성화한다.
5. PB0을 HIGH, PB1을 LOW로 설정하여 모터 1을 회전시킨다.
6. PB2를 HIGH, PB3을 LOW로 설정하여 모터 2를 회전시킨다.
7. 두 모터가 같은 방향으로 동작한다.
8. 3초마다 동일한 모터 제어 명령을 반복한다.

### 전체 동작 순서

```text
시스템 시작
     ↓
PORTB 출력 설정
     ↓
PORTB LOW 초기화
     ↓
ENA / ENB 활성화
     ↓
Motor 1
PB0 = HIGH
PB1 = LOW
     ↓
Motor 2
PB2 = HIGH
PB3 = LOW
     ↓
두 모터 회전
     ↓
3초 대기
     ↓
반복
```

### 동작 사진 / 영상

| 정면 동작 모습 |
| :---: |
|https://drive.google.com/drive/folders/1x8ByFjWSCfdfoOzbYv4eRb07Ov4UYSTp |

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)

본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool) | 활용 영역 | 세부 사용 목적 및 내용 |
| :--- | :--- | :--- |
| **ChatGPT** | 개념 정리 및 보고서 작성 | PORTB 출력 설정, 비트 연산 및 DC 모터 제어 동작 원리 참고 |

### AI 활용 및 검증 원칙

1. **본인 검증:** AI가 제공한 개념과 설명을 이해한 후 실제 코드와 회로의 동작을 비교하여 확인하였다.
2. **직접 검증:** 모터 드라이버의 입력 핀 상태와 실제 모터의 회전 방향을 확인하여 프로그램의 동작을 검증하였다.