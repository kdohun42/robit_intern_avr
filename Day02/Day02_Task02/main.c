#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "i2c_lcd.h"

#define SW1_A         PE4 // A값 증가 스위치
#define SW2_OPERATOR  PE5 // 연산자 변경 스위치
#define SW3_B         PD2 // B값 증가 스위치
#define SW4_RESULT    PD3 // 계산 결과 출력 스위치

// 스위치 핀을 입력으로 설정하고 내부 풀업 저항 활성화
void switch_init(void)
{
	DDRE &= ~((1 << PE4) | (1 << PE5)); // PE4, PE5를 입력으로 설정
	PORTE |= (1 << PE4) | (1 << PE5); // PE4, PE5 내부 풀업 저항 활성화
	
	DDRD &= ~((1 << PD2) | (1 << PD3)); // PD2, PD3을 입력으로 설정
	PORTD |= (1 << PD2) | (1 << PD3); // PD2, PD3 내부 풀업 저항 활성화
}


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


// LCD 첫 번째 줄의 내용을 공백으로 지움
void lcd_clear_first_line(void)
{
	i2c_lcd_goto_xy(0, 0); // LCD 첫 번째 줄로 이동
	i2c_lcd_string("                "); // 공백 16개 출력
}


// 계산 전 식을 LCD에 출력
void display_expression(unsigned int A, char operator, unsigned int B)
{
	char lcd[17]; // LCD 출력 문자열
	
	snprintf(lcd, sizeof(lcd), "%u%c%u=?", A, operator, B); // 계산식 문자열 생성
	
	lcd_clear_first_line(); // 첫 번째 줄 지우기
	i2c_lcd_goto_xy(0, 0); // 첫 번째 줄 처음으로 이동
	i2c_lcd_string(lcd); // 계산식 출력
}


// 계산 결과를 LCD에 출력
void display_result(unsigned int A, char operator, unsigned int B, long result)
{
	char lcd[17]; // LCD 출력 문자열
	
	snprintf(lcd, sizeof(lcd), "%u%c%u=%ld", A, operator, B, result); // 결과 문자열 생성
	
	lcd_clear_first_line(); // 첫 번째 줄 지우기
	i2c_lcd_goto_xy(0, 0); // 첫 번째 줄 처음으로 이동
	i2c_lcd_string(lcd); // 계산 결과 출력
}


// 선택된 연산자에 따라 A와 B를 계산
long calculate(unsigned int A, unsigned int B, char operator)
{
	long result = 0; // 계산 결과 저장
	
	if (operator == '+')
	{
		result = (long)A + (long)B; // 덧셈
	}
	else if (operator == '-')
	{
		result = (long)A - (long)B; // 뺄셈
	}
	else if (operator == '*')
	{
		result = (long)A * (long)B; // 곱셈
	}
	else if (operator == '/')
	{
		result = (long)A / (long)B; // 정수 나눗셈
	}
	
	return result; // 계산 결과 반환
}


int main(void)
{
	unsigned int A = 1, B = 1; // A와 B의 초깃값
	char operators[4] = {'+', '-', '*', '/'}; // 연산자 배열
	uint8_t operator_index = 0; // 현재 연산자 번호
	char current_operator = operators[operator_index]; // 현재 연산자
	long result; // 계산 결과
	
	i2c_lcd_init(); // LCD 초기화
	switch_init(); // 스위치 초기화
	
	display_expression(A, current_operator, B); // 처음 화면에 1+1=? 출력
	
	while (1)
	{
		if (switch_e_pressed(SW1_A)) // PE4 스위치를 누르면 실행
		{
			A++; // A값 1 증가
			display_expression(A, current_operator, B); // 변경된 계산식 출력
		}
		else if (switch_e_pressed(SW2_OPERATOR)) // PE5 스위치를 누르면 실행
		{
			operator_index++; // 연산자 번호 1 증가
			operator_index %= 4; // 4가 되면 다시 0으로 변경
			current_operator = operators[operator_index]; // 현재 연산자 변경
			
			display_expression(A, current_operator, B); // 변경된 계산식 출력
		}
		else if (switch_d_pressed(SW3_B)) // PD2 스위치를 누르면 실행
		{
			B++; // B값 1 증가
			display_expression(A, current_operator, B); // 변경된 계산식 출력
		}
		else if (switch_d_pressed(SW4_RESULT)) // PD3 스위치를 누르면 실행
		{
			if ((current_operator == '/') && (B == 0)) // 0으로 나누는지 확인
			{
				lcd_clear_first_line(); // 첫 번째 줄 지우기
				i2c_lcd_goto_xy(0, 0); // 첫 번째 줄 처음으로 이동
				i2c_lcd_string("DIV BY ZERO"); // 0으로 나누기 오류 출력
			}
			else
			{
				result = calculate(A, B, current_operator); // 계산 실행
				display_result(A, current_operator, B, result); // 계산 결과 출력
			}
		}
	}
	
	return 0;
}