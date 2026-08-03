# Day03_Task03

> **광운대학교 로봇학부**
> **작성자:** 김도훈
> **제출일:** 2026년 8월 2일

---

## 1. 개요 (Overview)

본 과제는 ATmega128 마이크로컨트롤러를 이용하여 Dynamixel 서보모터의 목표 위치와 이동 속도를 제어하는 시스템을 구현하는 것을 목표로 함.

가변저항을 ATmega128의 ADC0 채널에 연결하여 0부터 1023까지의 값을 측정하고, 측정된 값을 Dynamixel 모터의 목표 위치값으로 사용함. 또한 컴퓨터의 시리얼 터미널에서 숫자 `0`부터 `9`까지를 입력하면 해당 값을 0부터 300 사이의 속도값으로 변환하여 모터의 Profile Velocity에 적용함.

ATmega128의 USART0은 MAX485 모듈을 통한 Dynamixel Protocol 2.0 통신에 사용하고, USART1은 컴퓨터와의 속도 입력 통신에 사용함. 목표 속도와 목표 위치는 16×2 I2C LCD에 실시간으로 출력함.

### 핵심 목표

* ADC0을 이용한 가변저항 입력값 측정
* ADC 값을 Dynamixel 목표 위치값으로 사용
* PC에서 입력한 숫자를 목표 속도로 변환
* USART0과 MAX485를 이용한 Dynamixel 반이중 통신
* USART1을 이용한 컴퓨터 입력 데이터 수신
* Dynamixel Protocol 2.0 Write 패킷 생성
* CRC 계산 및 Byte Stuffing 처리
* Torque Enable, Profile Velocity, Goal Position 제어
* 위치 데드밴드를 이용한 불필요한 패킷 전송 감소
* I2C LCD를 이용한 목표 속도와 위치 출력

---

## 2. 개발 환경 (Environment)

| 항목                 | 내용                                                                          |
| :----------------- | :-------------------------------------------------------------------------- |
| **MCU**            | ATmega128A (16MHz External Crystal)                                         |
| **IDE / Compiler** | Microchip Studio 7.0 / Microchip AVR GCC                                    |
| **Flasher Tool**   | USBISP / STK500                                                             |
| **언어**             | C Language                                                                  |
| **통신 방식**          | USART0, USART1, RS-485, I2C                                                 |
| **Dynamixel 프로토콜** | Protocol 2.0                                                                |
| **주요 부품**          | ATmega128 개발보드, Dynamixel 모터, MAX485 모듈, 가변저항, 16×2 I2C LCD, USB to UART 모듈 |

---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]                         [Target Component]

PF0 / ADC0              <--------  가변저항 출력 단자

PE0 / RXD0              <--------  MAX485 RO
PE1 / TXD0              -------->  MAX485 DI
PE2                      -------->  MAX485 RE/DE

MAX485 A / B             <-------> Dynamixel 통신선

PD2 / RXD1              <--------  USB to UART 모듈 TX
PD3 / TXD1              -------->  USB to UART 모듈 RX

I2C 통신 핀             <-------> 16×2 I2C LCD
```

### 주요 회로 특징

* **전원:** ATmega128, MAX485, LCD 및 가변저항에는 안정적인 5V 전원을 공급함.
* **Dynamixel 전원:** Dynamixel 모터에는 모터 사양에 맞는 별도의 전원을 공급함.
* **공통 접지:** ATmega128, MAX485, USB to UART, LCD 및 Dynamixel 전원의 GND를 공통으로 연결함.
* **가변저항:** 양 끝 단자를 VCC와 GND에 연결하고 가운데 출력 단자를 PF0에 연결함.
* **ADC 입력:** PF0의 내부 풀업 저항을 비활성화하여 아날로그 입력값에 영향을 주지 않도록 함.
* **RS-485 통신:** MAX485의 A와 B 단자를 Dynamixel의 차동 통신선에 연결함.
* **방향 제어:** PE2를 이용하여 MAX485의 송신 모드와 수신 모드를 전환함.
* **USART0:** Dynamixel과 57600bps로 통신함.
* **USART1:** 컴퓨터와 9600bps로 통신함.
* **LCD:** I2C 통신을 이용하여 목표 속도와 목표 위치를 표시함.
* **주의사항:** Dynamixel 모터의 전원을 ATmega128 출력 핀에서 직접 공급하면 안 되며, 충분한 전류를 공급할 수 있는 별도의 전원을 사용해야 함.

---

## 4. 프로젝트 구조 (Directory Structure)

> 구현부(.c), 선언부(.h)만 구조에 표기함.

```text
├─ Day03_Task03
│   ├── main.c       # ADC, USART, Dynamixel 패킷 및 메인 제어 코드
│   ├── i2c_lcd.c    # I2C LCD 제어 함수 구현
│   ├── i2c_lcd.h    # I2C LCD 함수 선언
│   ├──  README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### Dynamixel 기본 설정값

```c
#define DXL_ID                      1

#define DXL_DIR_PIN                 PE2

#define DXL_TORQUE_ENABLE_ADDR      64
#define DXL_PROFILE_VELOCITY_ADDR   112
#define DXL_GOAL_POSITION_ADDR      116
```

`DXL_ID`는 제어할 Dynamixel 모터의 ID를 나타내며 현재 코드에서는 ID 1번 모터를 제어함.

각 주소는 Dynamixel의 Control Table에서 해당 기능이 저장된 주소를 의미함.

| 정의                          |  주소 | 기능                      |
| :-------------------------- | :-: | :---------------------- |
| `DXL_TORQUE_ENABLE_ADDR`    |  64 | 모터 토크 활성화 또는 비활성화       |
| `DXL_PROFILE_VELOCITY_ADDR` | 112 | 목표 위치까지 이동할 때 사용할 속도 설정 |
| `DXL_GOAL_POSITION_ADDR`    | 116 | 모터가 이동할 목표 위치 설정        |

Torque Enable에는 1바이트 값을 전송하고, Profile Velocity와 Goal Position에는 4바이트 값을 전송함.

### 제어 관련 설정값

```c
#define ADC_SAMPLE_COUNT            8
#define POSITION_DEADBAND           3

#define INITIAL_SPEED_INPUT         3
```

| 설정값                   | 기능                               |
| :-------------------- | :------------------------------- |
| `ADC_SAMPLE_COUNT`    | ADC 값을 8번 측정하여 평균 계산             |
| `POSITION_DEADBAND`   | 이전 위치와 현재 위치의 차이가 3 이상일 때만 위치 전송 |
| `INITIAL_SPEED_INPUT` | 프로그램 시작 시 사용할 PC 입력값             |

초기 속도 입력값은 3이며, 이를 0부터 300까지의 범위로 변환함.

```text
초기 속도 = 3 × 300 ÷ 9
          = 100
```

따라서 프로그램 시작 시 Dynamixel의 Profile Velocity에는 100이 설정됨.

### MAX485 송수신 모드 설정

```c
void RS485_TransmitMode(void)
{
	PORTE |= (1 << DXL_DIR_PIN);
}

void RS485_ReceiveMode(void)
{
	PORTE &= ~(1 << DXL_DIR_PIN);
}
```

RS-485 통신은 하나의 통신선을 송신과 수신에 함께 사용하는 반이중 통신 방식임.

MAX485의 `RE/DE` 핀을 PE2로 제어하여 송신 모드와 수신 모드를 전환함.

| PE2 상태 | MAX485 동작 |
| :----: | :-------- |
|  HIGH  | 송신 모드     |
|   LOW  | 수신 모드     |

Dynamixel에 패킷을 전송하기 전에는 송신 모드로 변경하고, 패킷 전송이 완료되면 수신 모드로 변경함.

### Dynamixel 통신용 USART0 초기화

```c
void Dynamixel_USART0_Init(void)
{
	DDRE |= (1 << DXL_DIR_PIN);

	RS485_ReceiveMode();

	UCSR0A = (1 << U2X0);

	UBRR0H = (uint8_t)(DXL_USART_UBRR >> 8);
	UBRR0L = (uint8_t)DXL_USART_UBRR;

	UCSR0B = (1 << RXEN0) | (1 << TXEN0);

	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}
```

PE2를 MAX485 방향 제어용 출력 핀으로 설정하고 초기 상태는 수신 모드로 설정함.

USART0은 Dynamixel과 통신하기 위해 57600bps로 설정함.

```c
#define DXL_USART_UBRR 34
```

`U2X0` 비트를 설정하여 USART0을 더블 스피드 모드로 동작시킴.

더블 스피드 모드의 통신 속도 계산식은 다음과 같음.

```text
Baud Rate
=
16,000,000
÷
(8 × (34 + 1))

≒ 57,143bps
```

설정된 통신 속도는 목표값인 57600bps와 근접함.

USART0의 통신 형식은 다음과 같음.

| 설정 항목  |    설정값   |
| :----- | :------: |
| 통신 속도  | 57600bps |
| 데이터 비트 |    8비트   |
| 패리티 비트 |    없음    |
| 정지 비트  |    1비트   |
| 송신 기능  |    활성화   |
| 수신 기능  |    활성화   |

### PC 통신용 USART1 초기화

```c
void PC_USART1_Init(void)
{
	UCSR1A = 0x00;

	UBRR1H = (uint8_t)(PC_USART_UBRR >> 8);
	UBRR1L = (uint8_t)PC_USART_UBRR;

	UCSR1B = (1 << RXEN1) | (1 << TXEN1);

	UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
}
```

USART1은 컴퓨터에서 속도 명령을 입력받는 용도로 사용함.

```c
#define PC_USART_UBRR 103
```

16MHz CPU 클럭에서 일반 속도 비동기 통신을 사용할 때 통신 속도는 다음과 같이 계산됨.

```text
Baud Rate
=
16,000,000
÷
(16 × (103 + 1))

≒ 9,615bps
```

목표 통신 속도는 9600bps이며, 시리얼 터미널도 9600bps로 설정해야 함.

### USART0 바이트 전송

```c
void USART0_SendByte(uint8_t data)
{
	while (!(UCSR0A & (1 << UDRE0)));

	UDR0 = data;
}
```

`UDRE0` 비트를 확인하여 USART0의 송신 버퍼가 비워질 때까지 기다림.

버퍼가 비워지면 전송할 바이트를 `UDR0` 레지스터에 저장함.

Dynamixel 패킷은 여러 개의 바이트로 구성되므로 완성된 패킷 배열을 한 바이트씩 전송할 때 사용함.

### USART0 수신 버퍼 제거

```c
void USART0_FlushReceive(void)
{
	volatile uint8_t dummy;

	while (UCSR0A & (1 << RXC0))
	{
		dummy = UDR0;
	}

	(void)dummy;
}
```

USART0 수신 버퍼에 이전 통신에서 남은 데이터가 존재하는 동안 `UDR0`를 반복해서 읽음.

Dynamixel에 새로운 패킷을 전송하기 전과 Status Packet을 기다린 후 호출하여 남아 있는 수신 데이터를 제거함.

### USART1 데이터 수신 확인

```c
uint8_t USART1_DataAvailable(void)
{
	if (UCSR1A & (1 << RXC1))
	{
		return 1;
	}

	return 0;
}
```

`RXC1` 비트를 확인하여 PC로부터 새로운 문자가 수신되었는지 확인함.

데이터가 수신되지 않은 경우 기다리지 않고 `0`을 반환하기 때문에 메인 반복문이 계속 실행될 수 있음.

```c
uint8_t USART1_ReceiveByte(void)
{
	return UDR1;
}
```

새로운 데이터가 존재할 때 `UDR1`을 읽어 수신된 문자를 반환함.

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

`ADMUX` 레지스터의 `REFS0` 비트를 설정하여 AVCC를 ADC 기준전압으로 사용함.

채널 선택 비트가 모두 0이므로 ADC0 채널이 선택됨.

`ADCSRA` 레지스터의 `ADEN` 비트를 설정하여 ADC를 활성화하고, `ADPS2~ADPS0`을 모두 1로 설정하여 분주비 128을 사용함.

```text
ADC 클럭 = 16MHz ÷ 128
         = 125kHz
```

ADC 초기화 직후의 첫 번째 측정값은 불안정할 수 있으므로 첫 번째 결과는 사용하지 않고 버림.

### ADC 값 읽기 및 평균 계산

```c
uint16_t ADC_Read(void)
{
	ADCSRA |= (1 << ADSC);

	while (ADCSRA & (1 << ADSC));

	return ADC;
}
```

`ADSC` 비트를 설정하여 ADC 변환을 시작하고, 변환이 완료될 때까지 기다린 후 10비트 ADC 결과를 반환함.

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

가변저항의 입력값이 미세하게 흔들리는 현상을 줄이기 위해 ADC 값을 8번 측정하여 평균값을 계산함.

ADC 평균값의 범위는 0부터 1023까지이며, 해당 값을 별도의 변환 없이 Dynamixel의 목표 위치값으로 사용함.

### Dynamixel CRC 계산

```c
uint16_t Dynamixel_UpdateCRC(
uint16_t crc_accum,
const uint8_t *data,
uint16_t data_size)
{
	uint16_t i;
	uint8_t j;

	for (i = 0; i < data_size; i++)
	{
		crc_accum ^= ((uint16_t)data[i] << 8);

		for (j = 0; j < 8; j++)
		{
			if (crc_accum & 0x8000)
			{
				crc_accum =
					(uint16_t)((crc_accum << 1) ^ 0x8005);
			}
			else
			{
				crc_accum =
					(uint16_t)(crc_accum << 1);
			}
		}
	}

	return crc_accum;
}
```

Dynamixel Protocol 2.0 패킷의 오류 검출을 위해 CRC 값을 계산함.

패킷의 각 바이트를 순서대로 확인하면서 CRC 누적값을 갱신함. 최상위 비트가 1인 경우 왼쪽으로 이동한 후 다항식 `0x8005`와 XOR 연산을 수행함.

계산된 16비트 CRC는 패킷의 마지막 두 바이트에 저장됨.

```text
CRC_L → CRC 하위 바이트
CRC_H → CRC 상위 바이트
```

수신한 Dynamixel은 동일한 방식으로 CRC를 계산하고, 패킷에 포함된 CRC와 비교하여 데이터 오류 여부를 확인함.

### Dynamixel Write 패킷 본문 생성

```c
body[0] = 0x03;
body[1] = (uint8_t)(address & 0xFF);
body[2] = (uint8_t)(address >> 8);
```

`0x03`은 Dynamixel Protocol 2.0의 Write 명령을 의미함.

Write 패킷의 본문은 다음 순서로 구성됨.

```text
Instruction
Address_L
Address_H
Data 0
Data 1
...
```

주소는 16비트 값이므로 하위 바이트와 상위 바이트로 나누어 저장함.

### Protocol 2.0 Byte Stuffing

```c
for (i = 0; i < body_length; i++)
{
	stuffed_body[stuffed_length] = body[i];
	stuffed_length++;

	if (stuffed_length >= 3)
	{
		if (stuffed_body[stuffed_length - 3] == 0xFF &&
		stuffed_body[stuffed_length - 2] == 0xFF &&
		stuffed_body[stuffed_length - 1] == 0xFD)
		{
			stuffed_body[stuffed_length] = 0xFD;
			stuffed_length++;
		}
	}
}
```

패킷 본문 안에 헤더와 같은 바이트 배열인 `FF FF FD`가 나타날 경우 Dynamixel이 이를 새로운 패킷의 헤더로 잘못 판단할 수 있음.

이를 방지하기 위해 `FF FF FD`가 발견되면 추가로 `FD`를 삽입함.

```text
원본 데이터: FF FF FD
전송 데이터: FF FF FD FD
```

이 과정을 Byte Stuffing이라고 함.

### Dynamixel 패킷 헤더 생성

```c
packet[0] = 0xFF;
packet[1] = 0xFF;
packet[2] = 0xFD;
packet[3] = 0x00;
packet[4] = DXL_ID;
packet[5] = (uint8_t)(length & 0xFF);
packet[6] = (uint8_t)(length >> 8);
```

Dynamixel Protocol 2.0 패킷은 다음과 같이 구성됨.

```text
Header 1     0xFF
Header 2     0xFF
Header 3     0xFD
Reserved     0x00
Packet ID
Length_L
Length_H
Instruction
Address_L
Address_H
Data
CRC_L
CRC_H
```

현재 코드에서는 ID가 1인 Dynamixel 모터에 Write 명령을 전송함.

Length 값에는 Byte Stuffing이 적용된 패킷 본문 길이와 CRC 2바이트가 포함됨.

### Dynamixel 패킷 전송

```c
USART0_FlushReceive();

RS485_TransmitMode();

_delay_us(10);

UCSR0A |= (1 << TXC0);

for (i = 0; i < packet_length; i++)
{
	USART0_SendByte(packet[i]);
}

while (!(UCSR0A & (1 << TXC0)));

RS485_ReceiveMode();

_delay_ms(3);

USART0_FlushReceive();
```

패킷을 전송하기 전에 USART0 수신 버퍼에 남아 있는 데이터를 제거함.

MAX485를 송신 모드로 변경한 후 패킷 배열을 처음부터 마지막까지 한 바이트씩 전송함.

`TXC0` 비트를 확인하여 마지막 바이트의 마지막 비트까지 전송이 완료될 때까지 기다림.

전송 완료 후 MAX485를 수신 모드로 변경하여 Dynamixel이 전송하는 Status Packet을 받을 수 있도록 함.

3ms 동안 대기한 후 수신된 Status Packet 데이터를 제거함.

현재 코드는 Status Packet의 내용을 분석하지 않고 수신 버퍼에서 제거함.

### 1바이트 데이터 전송

```c
void Dynamixel_Write1Byte(
uint16_t address,
uint8_t value)
{
	uint8_t data[1];

	data[0] = value;

	Dynamixel_SendWritePacket(address, data, 1);
}
```

Torque Enable과 같이 1바이트 크기의 값을 전송할 때 사용함.

```c
Dynamixel_Write1Byte(
DXL_TORQUE_ENABLE_ADDR,
1
);
```

값 `1`을 전송하면 토크가 활성화되고, 값 `0`을 전송하면 토크가 비활성화됨.

### 4바이트 데이터 전송

```c
void Dynamixel_Write4Byte(
uint16_t address,
uint32_t value)
{
	uint8_t data[4];

	data[0] = (uint8_t)(value & 0xFF);
	data[1] = (uint8_t)((value >> 8) & 0xFF);
	data[2] = (uint8_t)((value >> 16) & 0xFF);
	data[3] = (uint8_t)((value >> 24) & 0xFF);

	Dynamixel_SendWritePacket(address, data, 4);
}
```

Profile Velocity와 Goal Position은 4바이트 크기의 값이므로 `uint32_t` 값을 네 개의 바이트로 분리함.

Dynamixel은 Little Endian 방식으로 데이터를 사용하므로 가장 낮은 바이트부터 순서대로 저장함.

예를 들어 값 `0x12345678`은 다음 순서로 전송됨.

```text
0x78 → 0x56 → 0x34 → 0x12
```

### PC 입력값을 속도로 변환

```c
uint32_t Convert_PCInputToSpeed(uint8_t input_number)
{
	uint32_t speed;

	speed =
	((uint32_t)input_number * 300UL) / 9UL;

	return speed;
}
```

PC에서 입력한 숫자 0부터 9를 Dynamixel의 목표 속도 0부터 300으로 변환함.

변환식은 다음과 같음.

```text
목표 속도 = 입력 숫자 × 300 ÷ 9
```

| PC 입력 | 목표 속도 |
| :---: | :---: |
|   0   |   0   |
|   1   |   33  |
|   2   |   66  |
|   3   |  100  |
|   4   |  133  |
|   5   |  166  |
|   6   |  200  |
|   7   |  233  |
|   8   |  266  |
|   9   |  300  |

정수 연산을 사용하므로 소수점 이하는 버려짐.

### 두 위치값 차이 계산

```c
uint16_t AbsoluteDifference(
uint16_t value1,
uint16_t value2)
{
	if (value1 >= value2)
	{
		return value1 - value2;
	}

	return value2 - value1;
}
```

현재 ADC 위치값과 마지막으로 전송한 위치값의 차이를 절댓값으로 계산함.

두 위치값의 차이가 `POSITION_DEADBAND`인 3 이상일 때만 새로운 목표 위치를 Dynamixel에 전송함.

가변저항의 ADC 값은 노이즈로 인해 1 또는 2 정도 흔들릴 수 있으므로, 데드밴드를 사용하여 불필요한 패킷 전송을 감소시킴.

### LCD 문자열 출력

```c
void LCD_WriteLine(uint8_t row, const char *text)
{
	char line[17];
	uint8_t i;

	for (i = 0; i < 16; i++)
	{
		line[i] = ' ';
	}

	line[16] = '\0';

	i = 0;

	while (text[i] != '\0' && i < 16)
	{
		line[i] = text[i];
		i++;
	}

	i2c_lcd_goto_xy(row, 0);
	i2c_lcd_string(line);
}
```

16×2 LCD의 한 줄 전체를 공백으로 초기화한 후 출력할 문자열을 복사함.

기존에 출력된 긴 문자열의 일부가 LCD에 남는 현상을 방지하기 위해 항상 16칸을 채워 출력함.

### 목표 속도와 위치 LCD 출력

```c
void LCD_Display(
uint32_t target_speed,
uint16_t target_position)
{
	char line1[17];
	char line2[17];

	snprintf(
	line1,
	sizeof(line1),
	"SPEED: %3lu",
	(unsigned long)target_speed
	);

	snprintf(
	line2,
	sizeof(line2),
	"POSITION: %4u",
	(unsigned int)target_position
	);

	LCD_WriteLine(0, line1);
	LCD_WriteLine(1, line2);
}
```

LCD 첫 번째 줄에는 현재 목표 속도를 출력하고, 두 번째 줄에는 가변저항으로 설정된 목표 위치를 출력함.

출력 예시는 다음과 같음.

```text
SPEED: 100
POSITION:  512
```

### Dynamixel 초기 설정

```c
Dynamixel_Write1Byte(
DXL_TORQUE_ENABLE_ADDR,
0
);
```

Profile Velocity를 변경하기 전에 Torque Enable 주소에 0을 전송하여 토크를 비활성화함.

```c
Dynamixel_Write4Byte(
DXL_PROFILE_VELOCITY_ADDR,
target_speed
);
```

초기 목표 속도를 Profile Velocity 주소에 전송함.

```c
Dynamixel_Write1Byte(
DXL_TORQUE_ENABLE_ADDR,
1
);
```

설정이 완료되면 Torque Enable 주소에 1을 전송하여 모터 토크를 활성화함.

```c
Dynamixel_Write4Byte(
DXL_GOAL_POSITION_ADDR,
target_position
);
```

가변저항에서 읽은 초기 ADC 값을 Goal Position 주소에 전송하여 모터를 초기 위치로 이동시킴.

### PC 속도 입력 처리

```c
if (USART1_DataAvailable())
{
	received_data = USART1_ReceiveByte();

	if (received_data >= '0' &&
	received_data <= '9')
	{
		speed_input =
			(uint8_t)(received_data - '0');

		target_speed =
			Convert_PCInputToSpeed(speed_input);

		Dynamixel_Write4Byte(
		DXL_PROFILE_VELOCITY_ADDR,
		target_speed
		);

		force_position_send = 1;
	}
}
```

USART1에 데이터가 수신되면 입력 문자가 `'0'`부터 `'9'` 사이인지 확인함.

ASCII 문자에서 `'0'`을 빼 실제 숫자값으로 변환함.

변환된 숫자는 0부터 300 사이의 목표 속도로 변환되어 Dynamixel의 Profile Velocity 주소에 전송됨.

속도가 변경되면 새로운 속도를 실제 이동에 적용하기 위해 `force_position_send`를 1로 설정함.

### 목표 위치 전송

```c
target_position = ADC_ReadAverage();

if (force_position_send ||
AbsoluteDifference(
target_position,
last_position
) >= POSITION_DEADBAND)
{
	Dynamixel_Write4Byte(
	DXL_GOAL_POSITION_ADDR,
	target_position
	);

	last_position = target_position;

	force_position_send = 0;
}
```

가변저항에서 측정한 ADC 평균값을 목표 위치로 사용함.

현재 목표 위치와 마지막으로 전송한 위치의 차이가 3 이상인 경우 새로운 Goal Position을 전송함.

속도가 변경된 경우에는 위치 차이가 3보다 작더라도 `force_position_send`에 의해 목표 위치를 다시 전송함.

이는 Dynamixel이 변경된 Profile Velocity를 적용하여 현재 목표 위치로 다시 이동하도록 하기 위한 처리임.

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오

1. 프로그램이 시작되면 I2C LCD를 초기화함.
2. PF0을 ADC0 입력으로 설정하고 가변저항 측정을 위한 ADC를 초기화함.
3. PC 통신용 USART1을 9600bps로 초기화함.
4. Dynamixel 통신용 USART0을 더블 스피드 모드의 57600bps로 초기화함.
5. PE2를 MAX485 방향 제어 출력으로 설정하고 수신 모드로 초기화함.
6. Dynamixel의 전원과 통신이 안정화될 수 있도록 500ms 동안 대기함.
7. 초기 입력값 3을 목표 속도 100으로 변환함.
8. 가변저항의 초기 ADC 평균값을 읽어 목표 위치로 설정함.
9. LCD에 초기 목표 속도와 목표 위치를 출력함.
10. Dynamixel의 토크를 비활성화함.
11. Profile Velocity에 초기 목표 속도 100을 설정함.
12. Dynamixel의 토크를 다시 활성화함.
13. 가변저항의 초기 ADC 값을 Goal Position으로 전송함.
14. PC에서 숫자 `0`부터 `9` 중 하나가 입력되었는지 계속 확인함.
15. 숫자가 입력되면 0부터 300 사이의 목표 속도로 변환함.
16. 변환된 목표 속도를 Dynamixel의 Profile Velocity에 전송함.
17. 변경된 속도를 적용하기 위해 현재 목표 위치를 다시 전송함.
18. 가변저항의 ADC 값을 8번 측정하여 평균값을 계산함.
19. 현재 위치와 마지막으로 전송한 위치의 차이가 3 이상이면 새로운 Goal Position을 전송함.
20. LCD에 현재 목표 속도와 목표 위치를 출력함.
21. 위 과정을 50ms 간격으로 반복함.

### 초기 동작 예시

프로그램 시작 시 PC 속도 입력값은 3으로 설정되어 있음.

```text
입력값: 3
목표 속도: 100
```

가변저항의 ADC 값이 512인 경우 LCD에는 다음과 같이 출력됨.

```text
SPEED: 100
POSITION:  512
```

Dynamixel에는 다음 설정이 순서대로 전송됨.

```text
Torque Enable = 0
Profile Velocity = 100
Torque Enable = 1
Goal Position = 512
```

### PC 속도 입력 예시

PC 시리얼 터미널에서 숫자 `6`을 입력하면 다음과 같이 계산됨.

```text
목표 속도 = 6 × 300 ÷ 9
          = 200
```

LCD 출력은 다음과 같이 변경됨.

```text
SPEED: 200
POSITION:  512
```

Profile Velocity에 200이 전송되고, 변경된 속도를 적용하기 위해 현재 Goal Position도 다시 전송됨.

### 최대 속도 입력 예시

PC에서 숫자 `9`를 입력한 경우 다음과 같이 동작함.

```text
입력값: 9
목표 속도: 300
```

LCD 출력 예시는 다음과 같음.

```text
SPEED: 300
POSITION:  700
```

### 정지 속도 입력 예시

PC에서 숫자 `0`을 입력하면 목표 속도는 0으로 변환됨.

```text
입력값: 0
목표 속도: 0
```

Dynamixel의 Profile Velocity 주소에 0이 전송됨.

Profile Velocity에서 0의 실제 동작 의미는 사용 중인 Dynamixel 모델과 동작 모드의 Control Table 설정을 기준으로 확인해야 함.

### 가변저항 위치 제어 예시

가변저항의 ADC 값이 다음과 같이 변경되었다고 가정함.

```text
기존 전송 위치: 500
현재 목표 위치: 501
위치 차이: 1
```

위치 차이가 데드밴드 3보다 작으므로 새로운 Goal Position을 전송하지 않음.

```text
기존 전송 위치: 500
현재 목표 위치: 504
위치 차이: 4
```

위치 차이가 3 이상이므로 Goal Position에 504를 전송함.

### LCD 출력 예시

```text
SPEED:  33
POSITION:  120
```

```text
SPEED: 166
POSITION:  512
```

```text
SPEED: 300
POSITION: 1023
```

LCD의 첫 번째 줄에는 PC 입력으로 설정한 목표 속도가 표시되고, 두 번째 줄에는 가변저항에서 읽은 목표 위치가 표시됨.

### 동작 사진 / 영상

|                  동작 영상                 |
| :------------------------------------: |
| 코드는 작성했으나 제어를 성공하지 못해 영상을 촬용하지못하였습니다. 빠른 시일 내에 제출하겠습니다. |

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)

본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool)  | 활용 영역    | 세부 사용 목적 및 내용                                        |
| :---------- | :------- | :--------------------------------------------------- |
| **ChatGPT** | 개념 정리    | USART, ADC, RS-485 및 Dynamixel Protocol 2.0 동작 원리 참고 |
| **ChatGPT** | 패킷 구성 참고 | Write 명령, 패킷 헤더, Length, CRC 및 Byte Stuffing 구현 참고   |
| **ChatGPT** | 코드 구성 참고 | MAX485 송수신 방향 전환, 1바이트 및 4바이트 데이터 전송 방법 참고           |
| **ChatGPT** | 제어 로직 참고 | 가변저항 위치 제어, PC 속도 입력 변환 및 위치 데드밴드 처리 참고              |
| **ChatGPT** | 문서 작성 참고 | 프로그램의 동작 과정과 주요 레지스터 설정 내용 정리                        |

### AI 활용 및 검증 원칙

1. AI가 제공한 ADC, USART, RS-485 및 Dynamixel 통신 관련 개념을 이해한 후 코드를 작성함.
2. ATmega128 데이터시트를 통해 USART0, USART1 및 ADC 관련 레지스터 설정값을 확인함.
3. Dynamixel Protocol 2.0 문서를 통해 패킷 구조, Write 명령, CRC 및 Byte Stuffing 방식을 확인함.
4. 사용한 Dynamixel 모델의 Control Table에서 Torque Enable, Profile Velocity 및 Goal Position 주소를 확인함.
5. MAX485의 송신 모드와 수신 모드가 PE2 출력에 따라 정상적으로 전환되는지 확인함.
6. PC에서 숫자 0부터 9까지 입력하여 목표 속도가 0부터 300까지 변환되는지 확인함.
7. 가변저항을 회전시켜 ADC 값과 Dynamixel 목표 위치가 함께 변경되는지 확인함.
8. ADC 값의 작은 흔들림에서 불필요한 위치 패킷이 반복 전송되지 않는지 확인함.
9. LCD에 목표 속도와 목표 위치가 정상적으로 출력되는지 확인함.
10. Dynamixel ID, 통신 속도, 프로토콜 버전 및 동작 모드가 코드 설정과 일치하는지 확인함.
11. AI가 제시한 내용을 그대로 사용하지 않고 실제 하드웨어 환경과 모터 사양에 맞게 수정하고 검증함.
