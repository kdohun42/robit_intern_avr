/*
 * Day03_Task04.c
 *
 * Created: 2026-08-02 오전 3:12:46
 * Author : PC User
 */ 

/*
기본 세팅: TX - 출력, RX - 입력으로 설정, TX - HIGH(1)상태

전송 단계 : 시작 비트 -> 데이터 비트 -> 끝 비트
TX를 LOW로 바꿈
클럭 만큼 대기

*/
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

unsigned char data = 0b00000000;

void init(void){
	DDRE = 0b00000010; // PE0(RX)를 입력(0), PE1(TX)를 출력(1)으로 설정한다.
	PORTE = 0b00000010; // PE1(TX)를 HIGH로 설정
}

void TX(unsigned char data){
	PORTE = 0b00000000; // PE1(TX)를 LOW로 설정
	_delay_us(104); // 104 마이크로초 만큼 대기 = 보드레이트 9600 만큼 대기
	
	for(int i = 0; i < 8; i++){
		if(data & (1 << i)){ // 데이터가 같은
			PORTE |= (1 << PE1);
		}
		else{ // 데이터가 다름
			 PORTE &= ~(1 << PE1);
		}
		_delay_us(104);
		//data & (1 << i);
	}
	PORTE = 0b00000010; // PE1(TX)를 HIGH로 설정
	_delay_us(104);
}

unsigned char RX(void){
	while(1){
	unsigned char data = 0b00000000;
	while(PINE & 0b00000001){}// RX가 LOW인지 확인하기
 
		_delay_us(52);
		
		if(PINE & 0b00000001){
			continue;
		}
				for (int i = 0; i < 8; i++){
					
					_delay_us(104);
					
					if (PINE & 0b00000001){
						data |= (1 << i);
					}
				}
		}
		
		_delay_us(104);
		if(PINE & 0b00000001){
			return data;
		}
}

void TX_string(char *data){
	while(*data != '\0'){
		TX(*data++);
	}
}

//char data = "HelloWORld!";

// helloworld 아스키코드 값 배열
// unsigned char HELLOWORLd[10] = {0x68, 0x65, 0x6C, 0x6C, 0x6F, 0x77, 0x6F, 0x72, 0x6C, 0x64};

int main(void)
{
    /* Replace with your application code */

	init();
	
    while (1) 
    {
		TX_string("HelloWorld!\r\n");
		_delay_ms(1000);
    }
}

