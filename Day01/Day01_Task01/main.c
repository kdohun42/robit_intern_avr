#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

char led[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

int main(void)
{
	DDRA = 0xFF; // PINA 모두 출력으로 설정
	DDRE = 0x00; // PINE 모두 입력으로 설정
	PORTE = 0x03; // 내부 풀업 저항 활성화
	EICRA = 0xA0; // 10100000 2번 3번 인터럽트 falling edge로 설정
	EIMSK = 0x0C; // 00001100 2번 3번 인터럽트 활성화
	
	sei();
	
	while (1)
	{
		if (!(PINE & (1 << PINE4)) && !(PINE & (1 << PINE5))) // PE4와 PE5 눌렀을 때 실행
		{
			PORTA = 0x00; // active low 방식 00000000일 때 LED 출력
		}
		else if (!(PINE & (1 << PINE4))) // PE4 눌렀을 때 실행
		{
			PORTA = 0x0F; // 00001111 
		}
		else if (!(PINE & (1 << PINE5))) // PE5 눌렀을 때 실행
		{
			PORTA = 0xF0; // 11110000
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
}
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