#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "i2c_lcd.h"

#define LCD_ADDRESS		0x27 // PCF8574의 I2C 주소
#define LCD_RS			0x01 // 명령어와 문자 데이터 구분
#define LCD_RW			0x02 // LCD 읽기 및 쓰기 설정
#define LCD_EN			0x04 // LCD 데이터 입력 신호
#define LCD_BACKLIGHT	0x08 // LCD 백라이트 켜기


// ATmega128의 I2C 통신을 초기화
static void i2c_init(void)
{
	TWSR = 0x00; // I2C 분주비 1로 설정
	TWBR = 72; // I2C 통신 속도를 약 100kHz로 설정
	TWCR = (1 << TWEN); 
}


// I2C 통신의 시작 신호를 전송
static void i2c_start(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN); // START 신호 전송
	
	while (!(TWCR & (1 << TWINT))); // START 신호 전송 완료까지 대기
}


// I2C 장치에 1바이트 데이터를 전송
static void i2c_write(uint8_t data)
{
	TWDR = data; // 전송할 데이터를 TWDR에 저장
	TWCR = (1 << TWINT) | (1 << TWEN); // 데이터 전송 시작
	
	while (!(TWCR & (1 << TWINT))); // 데이터 전송 완료까지 대기
}


// I2C 통신의 종료 신호를 전송
static void i2c_stop(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); // STOP 신호 전송
	
	while (TWCR & (1 << TWSTO)); // STOP 신호 처리 완료까지 대기
}


// PCF8574로 데이터를 전송
static void pcf8574_write(uint8_t data)
{
	i2c_start(); // I2C 통신 시작
	i2c_write(LCD_ADDRESS << 1); // LCD 주소와 쓰기 비트 전송
	i2c_write(data); // PCF8574로 데이터 전송
	i2c_stop(); // I2C 통신 종료
}


// LCD의 EN 핀에 펄스를 발생시켜 데이터를 입력
static void lcd_enable_pulse(uint8_t data)
{
	pcf8574_write(data | LCD_EN); // EN 핀을 HIGH로 설정
	_delay_us(1);
	
	pcf8574_write(data & ~LCD_EN); // EN 핀을 LOW로 설정
	_delay_us(50);
}


// LCD에 4비트 데이터를 전송
static void lcd_write_4bit(uint8_t data, uint8_t mode)
{
	uint8_t output;
	
	output = (data & 0xF0) | LCD_BACKLIGHT | mode; // 상위 4비트와 LCD 설정 결합
	lcd_enable_pulse(output); // LCD에 데이터 입력
}


// LCD에 8비트 데이터를 상위와 하위 4비트로 나누어 전송
static void lcd_write_byte(uint8_t data, uint8_t mode)
{
	lcd_write_4bit(data & 0xF0, mode); // 상위 4비트 전송
	lcd_write_4bit((data << 4) & 0xF0, mode); // 하위 4비트 전송
}


// LCD에 명령어를 전송
void i2c_lcd_command(uint8_t command)
{
	lcd_write_byte(command, 0x00); // RS를 0으로 설정하고 명령어 전송
	
	if ((command == 0x01) || (command == 0x02)) // 화면 지우기 또는 커서 복귀 명령 확인
	{
		_delay_ms(2); // LCD 명령 처리 완료까지 대기
	}
}


// LCD에 문자 데이터를 전송
void i2c_lcd_data(uint8_t data)
{
	lcd_write_byte(data, LCD_RS); // RS를 1로 설정하고 문자 전송
}


// I2C LCD를 4비트 모드로 초기화
void i2c_lcd_init(void)
{
	i2c_init(); // I2C 통신 초기화
	_delay_ms(50); // LCD 전원 안정화 대기
	
	lcd_write_4bit(0x30, 0x00); // LCD 초기화 신호 전송
	_delay_ms(5);
	
	lcd_write_4bit(0x30, 0x00); // LCD 초기화 신호 다시 전송
	_delay_us(150);
	
	lcd_write_4bit(0x30, 0x00); // LCD 초기화 신호 다시 전송
	_delay_us(150);
	
	lcd_write_4bit(0x20, 0x00); // LCD를 4비트 모드로 설정
	_delay_us(150);
	
	i2c_lcd_command(0x28); // 4비트, 2줄, 5×8 글꼴 설정
	i2c_lcd_command(0x08); // LCD 화면 끄기
	i2c_lcd_command(0x01); // LCD 화면 전체 지우기
	i2c_lcd_command(0x06); // 문자 출력 후 커서를 오른쪽으로 이동
	i2c_lcd_command(0x0C); // 화면 켜기, 커서와 깜빡임 끄기
}


// LCD 화면의 모든 문자를 지움
void i2c_lcd_clear(void)
{
	i2c_lcd_command(0x01); // 화면 지우기 명령 전송
}


// LCD의 출력 위치를 설정
void i2c_lcd_goto_xy(uint8_t row, uint8_t column)
{
	uint8_t address;
	
	if (row == 0)
	{
		address = 0x00 + column; // LCD 첫 번째 줄 주소 계산
	}
	else
	{
		address = 0x40 + column; // LCD 두 번째 줄 주소 계산
	}
	
	i2c_lcd_command(0x80 | address); // LCD 커서를 계산한 위치로 이동
}


// 문자열을 한 글자씩 LCD에 출력
void i2c_lcd_string(const char *string)
{
	while (*string != '\0') // 문자열의 마지막 문자가 나올 때까지 반복
	{
		i2c_lcd_data(*string); // 현재 문자 출력
		string++; // 다음 문자로 이동
	}
}