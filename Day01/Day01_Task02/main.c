#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

char led[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
unsigned char number = 0b11111111;

int main(void)
{

	DDRA = 0xFF; // A핀 모두 출력으로 설정
	DDRE = 0x00; // E핀 모두 입력으로 설정
	DDRD = 0x00; // D핀 모두 입력으로 설정
	EICRB = 0b00001010; // 인터럽트 4번 5번 falling edge로 설정 
	EICRA = 0b10100000; // 인터럽트 2번 3번 falling edge로 설정 
	EIMSK = 0b00111100; // 2번 3번 4번 5번 활성화
	
	sei();
	
	// 2진 카운터 항상 반복
	while (1)
	{
		PORTA = number;
		_delay_ms(100);
		number--;
	}
}
// 인터럽트 2번 LED 3개씩 1개씩 이동(우측) 
ISR(INT2_vect){
	PORTA = 0b00011111;
	for(int i = 0; i < 6; i++){
		_delay_ms(1000);
		PORTA = (PORTA >> 1) | 0b10000000;
	}
}
// 인터럽트 3번 LED 3개씩 1칸씩 이동(좌측)
ISR(INT3_vect){
	PORTA = 0b11111000;
	for(int i = 0; i < 6; i++){
		_delay_ms(1000);
		PORTA = (PORTA << 1) | 0b00000001;
	}
}
//인터럽트 5번 LED 1번 ~ 8번 순참 점등 후 역순참 점등
ISR(INT5_vect){
	for(int i = 0; i < 8; i++){
		_delay_ms(100);
		PORTA =~ led[i];
	}
	for(int i = 7; i  >=0; i--){
		_delay_ms(100);
		PORTA =~ led[i];
	}
}
// 인터럽트 4번 2진 카운터 초기화
ISR(INT4_vect){
	number = 0xFF;
}
