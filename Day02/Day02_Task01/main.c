#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "i2c_lcd.h"

char led[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}; // LED 위치 배열

void adc_init(void)
{
	ADMUX = (1 << REFS0); // 기준 전압 AVCC, ADC 결과 오른쪽 정렬
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // ADC 활성화, 분주비 128
}

uint16_t adc_read(uint8_t channel)
{
	ADMUX = (ADMUX & 0xE0) | (channel & 0x1F); // ADC 채널 선택
	ADCSRA |= (1 << ADSC); // ADC 변환 시작
	
	while (ADCSRA & (1 << ADSC)); // ADC 변환이 끝날 때까지 대기
	
	return ADC; // ADC 변환값 반환
}

int main(void)
{
	char lcd[17]; // LCD 출력 문자열
	uint16_t adc_value; // ADC 변환값
	unsigned long voltage_x100; // 전압을 100배 한 값
	unsigned int volts, frac; // 전압의 정수 부분과 소수 부분
	uint8_t led_position; // 켜질 LED 위치
	
	DDRA = 0xFF; // PORTA 전체를 LED 출력으로 설정
	PORTA = 0xFF; // Active Low 방식에서 모든 LED 끄기
	
	i2c_lcd_init(); // LCD 초기화
	adc_init(); // ADC 초기화
	
	i2c_lcd_goto_xy(0, 0); // LCD 첫 번째 줄로 이동
	i2c_lcd_string("21th_KDH"); // 이름 및 이니셜 출력
	
	while (1)
	{
		adc_value = adc_read(0); // PF0의 ADC값 읽기
		led_position = adc_value / 128; // ADC값을 0~7의 LED 위치로 변환
		PORTA =~led[led_position]; // 해당 위치 LED 하나 켜기
		
		voltage_x100 = (unsigned long)adc_value * 500UL / 1023UL; // ADC값을 전압으로 변환
		volts = voltage_x100 / 100; // 전압의 정수 부분 계산
		frac = voltage_x100 % 100; // 전압의 소수 부분 계산
		
		snprintf(lcd, sizeof(lcd), "%4u %u.%02uV", adc_value, volts, frac); // 출력 문자열 만들기
		
		i2c_lcd_goto_xy(1, 0); // LCD 두 번째 줄로 이동
		i2c_lcd_string(lcd); // ADC값과 전압 출력
		
		_delay_ms(200); // 0.2초 대기
	}
	
	return 0;
}