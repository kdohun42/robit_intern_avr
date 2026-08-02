#define F_CPU 16000000UL // 16MHz 클럭 설정
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// UART0 초기화 함수 (보레이트: 9600)
void UART0_Init(unsigned int ubrr) {
	// 속도 설정 (UBRR0H, UBRR0L)
	UBRR0H = (unsigned char)(ubrr >> 8);
	UBRR0L = (unsigned char)ubrr;
	
	// 송신(TX), 수신(RX) 허용
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	
	// 프레임 형식 설정: 데이터 8비트, 정지 1비트
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// 1바이트 송신 함수
void UART0_Transmit(unsigned char data) {
	// 송신 버퍼가 비어있을 때까지 대기
	while (!(UCSR0A & (1 << UDRE0)));
	// 데이터 전송
	UDR0 = data;
}

// 1바이트 수신 함수
unsigned char UART0_Receive(void) {
	// 데이터가 수신될 때까지 대기
	while (!(UCSR0A & (1 << RXC0)));
	// 수신된 데이터 반환
	return UDR0;
}

// 문자열 송신 함수
void UART0_Print(char *str) {
	while (*str) {
		UART0_Transmit(*str++);
	}
}


unsigned char led[8] = {0b11111110, 0b11111101, 0b11111011, 0b11110111,0b11101111, 0b11011111,0b10111111, 0b01111111};
unsigned int ubrr;

int main() {
	
	DDRA = 0b11111111;
	PORTA = 0b11111111;
	DDRE = 0b00000000;
	EIMSK = 0b00010000;
	EICRB = 0b00000010;
	
	sei();
	UART0_Init(103);
	
	while (1) {
		// 수신된 데이터를 그대로 송신
		unsigned char rxData = UART0_Receive();
		
		if( rxData == '0'){
			UART0_Print("0Led on\r\n");
			PORTA = 0b11111110;
		}
		else if( rxData == '1'){
			UART0_Print("1Led on\r\n");
			PORTA = 0b11111101;
		}
		else if( rxData == '2'){
			UART0_Print("2Led on\r\n");
			PORTA = 0b11111011;
		}
		else if( rxData == '3'){
			UART0_Print("3Led on\r\n");
			PORTA = 0b11110111;
		}
		else if(rxData == '4'){
			UART0_Print("4Led on\r\n");
			PORTA = 0b11101111;
		}
		else if( rxData == '5'){
			UART0_Print("5Led on\r\n");
			PORTA = 0b11011111;
		}
		else if( rxData == '6'){
			UART0_Print("6Led on\r\n");
			PORTA = 0b10111111;
		}
		else if(rxData == '7'){
			UART0_Print("7Led on\r\n");
			PORTA = 0b01111111;
		}
		else if(rxData == '8'){
			UART0_Print("LEFT\r\n");
			for(int i = 0; i < 8; i++){
				_delay_ms(200);
				PORTA = led[i];
			}
		}
		else if(rxData == '9'){
			UART0_Print("RIGHT\r\n");
			for(int i = 7; i >= 0; i--){
				_delay_ms(200);
				PORTA = led[i];
			}
		}
		else{
			UART0_Print("ERROR\r\n");
			
			
		}
	}
}

ISR(INT4_vect){
	UART0_Print("RESET\r\n");
	UART0_Init(103);
}
