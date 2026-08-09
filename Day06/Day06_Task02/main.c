#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
	DDRB = 0x6F;   // 출력 설정

	// 전부 LOW
	PORTB = 0x00;

	// ENA, ENB 항상 활성화
	PORTB |= (1 << PB5) | (1 << PB6);

	while (1)
	{

		// 모터 1
		PORTB |=  (1 << PB0);
		PORTB &= ~(1 << PB1);

		// 모터 2
		PORTB |=  (1 << PB2);
		PORTB &= ~(1 << PB3);

		_delay_ms(3000);

	}
}