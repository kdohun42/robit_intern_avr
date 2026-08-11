#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>


/* =========================================================
   사용자 설정
   ========================================================= */

/*
    검은색에서 ADC 값이

    1 : 작아짐
    0 : 커짐

    검은 선에 올렸는데 LED가 반대로 동작하면
    1 ↔ 0 변경
*/
#define BLACK_IS_LOW           1


/*
    LED 동작 방식

    1 : PA 핀 HIGH → LED ON
    0 : PA 핀 LOW  → LED ON
*/
#define LED_ACTIVE_HIGH        1


/*
    모터 방향 반전
*/
#define LEFT_MOTOR_REVERSED    0
#define RIGHT_MOTOR_REVERSED   0


/*
    모터 속도
*/
#define FORWARD_SPEED          150
#define PIVOT_SPEED            140


/*
    라인 중앙 허용 범위
*/
#define CENTER_DEADBAND        700


/*
    검은 선 판단 기준
*/
#define LINE_DETECT_THRESHOLD  300


/*
    작은 센서값 제거
*/
#define SENSOR_NOISE_FLOOR     40


/*
    캘리브레이션 최소 ADC 변화량
*/
#define MIN_CAL_RANGE          40


/*
    센서 개수
*/
#define SENSOR_COUNT           6



/* =========================================================
   L298N 모터 드라이버
   ========================================================= */

#define LEFT_IN1       PB0
#define LEFT_IN2       PB1

#define RIGHT_IN1      PB2
#define RIGHT_IN2      PB3

#define LEFT_PWM_PIN   PB5
#define RIGHT_PWM_PIN  PB6



/* =========================================================
   스위치
   ========================================================= */

#define SW1_PIN        PE4
#define SW2_PIN        PE5



/* =========================================================
   LED
   ========================================================= */

#define LED1           PA0
#define LED2           PA1
#define LED3           PA2
#define LED4           PA3
#define LED5           PA4
#define LED6           PA5



/* =========================================================
   실제 IR 센서 배열

   실제 왼쪽 → 오른쪽

   PF1 PF2 PF3 PF6 PF5 PF4
   ========================================================= */

const uint8_t sensor_channel[SENSOR_COUNT] =
{
    1, 2, 3, 6, 5, 4
};



/* =========================================================
   센서 가중치
   ========================================================= */

const int16_t sensor_weight[SENSOR_COUNT] =
{
    -2500,
    -1500,
     -500,
      500,
     1500,
     2500
};



/* =========================================================
   센서 데이터
   ========================================================= */

uint16_t sensor_raw[SENSOR_COUNT];

uint16_t sensor_min[SENSOR_COUNT];
uint16_t sensor_max[SENSOR_COUNT];

uint16_t sensor_normalized[SENSOR_COUNT];



/* =========================================================
   상태 변수
   ========================================================= */

uint8_t calibration_done = 0;

uint8_t running = 0;


/*
    마지막 라인 방향

    -1 = 왼쪽
     0 = 중앙 / 모름
     1 = 오른쪽
*/
int8_t last_direction = 0;



/* =========================================================
   가로선 판단 변수
   ========================================================= */

/*
    짧은 시간 동안
    어떤 IR 센서가 검은색을 보았는지 기록
*/
uint8_t cross_seen = 0;


/*
    현재 가로선 후보를 검사 중인지
*/
uint8_t cross_checking = 0;


/*
    가로선 검사 시간
*/
uint8_t cross_timer = 0;


/*
    왼쪽 끝 IR1 검출 기록
*/
uint8_t left_seen = 0;


/*
    오른쪽 끝 IR6 검출 기록
*/
uint8_t right_seen = 0;


/*
    많은 센서가 동시에 들어온 적이 있는지
*/
uint8_t wide_seen = 0;


/*
    같은 가로선을 여러 번 세지 않도록 하는 잠금

    0 = 새로운 가로선 검출 가능
    1 = 방금 가로선을 검출함
*/
uint8_t cross_latched = 0;


/*
    지금까지 통과한 가로선 개수

    0 = 아직 없음
    1 = 첫 번째 가로선
    2 = 두 번째 가로선
    3 = 세 번째 가로선
    ...
*/
int HORIZON_COUNT = 0;



/* =========================================================
   함수 선언

   Line_Control()보다 아래쪽에 정의되는
   가로선 관련 함수를 미리 선언
   ========================================================= */

uint8_t IR_COUNT(uint8_t mask);

uint8_t HORIZON_COUNTER(uint8_t sensor_mask);

void ZONE_3(void);



/* =========================================================
   JTAG 비활성화
   ========================================================= */

void JTAG_Disable(void)
{
    uint8_t old_sreg;
    uint8_t value;

    old_sreg = SREG;

    cli();

    value = MCUCSR | (1 << JTD);

    MCUCSR = value;
    MCUCSR = value;

    SREG = old_sreg;
}



/* =========================================================
   ADC 초기화
   ========================================================= */

void ADC_Init(void)
{
    DDRF &= ~(
        (1 << PF1) |
        (1 << PF2) |
        (1 << PF3) |
        (1 << PF4) |
        (1 << PF5) |
        (1 << PF6)
    );


    PORTF &= ~(
        (1 << PF1) |
        (1 << PF2) |
        (1 << PF3) |
        (1 << PF4) |
        (1 << PF5) |
        (1 << PF6)
    );


    ADMUX = (1 << REFS0);


    ADCSRA =
        (1 << ADEN) |
        (1 << ADPS2) |
        (1 << ADPS1) |
        (1 << ADPS0);
}



/* =========================================================
   ADC 1회 변환
   ========================================================= */

uint16_t ADC_Convert_Once(void)
{
    uint8_t low;
    uint8_t high;


    ADCSRA |= (1 << ADSC);


    while (ADCSRA & (1 << ADSC))
    {
        ;
    }


    low = ADCL;

    high = ADCH;


    return ((uint16_t)high << 8) | low;
}



/* =========================================================
   특정 ADC 채널 읽기
   ========================================================= */

uint16_t ADC_Read(uint8_t channel)
{
    ADMUX =
        (1 << REFS0) |
        (channel & 0x07);


    /*
        채널 변경 직후 첫 값 버림
    */
    ADC_Convert_Once();


    return ADC_Convert_Once();
}



/* =========================================================
   IR 센서 6개 읽기
   ========================================================= */

void Read_All_Sensors(void)
{
    uint8_t i;


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        sensor_raw[i] =
            ADC_Read(sensor_channel[i]);
    }
}



/* =========================================================
   LED 초기화
   ========================================================= */

void LED_Init(void)
{
    DDRA |=
        (1 << LED1) |
        (1 << LED2) |
        (1 << LED3) |
        (1 << LED4) |
        (1 << LED5) |
        (1 << LED6);


#if LED_ACTIVE_HIGH

    PORTA &= ~(
        (1 << LED1) |
        (1 << LED2) |
        (1 << LED3) |
        (1 << LED4) |
        (1 << LED5) |
        (1 << LED6)
    );

#else

    PORTA |=
        (1 << LED1) |
        (1 << LED2) |
        (1 << LED3) |
        (1 << LED4) |
        (1 << LED5) |
        (1 << LED6);

#endif
}



/* =========================================================
   LED ON
   ========================================================= */

void LED_On(uint8_t led)
{
#if LED_ACTIVE_HIGH

    PORTA |= (1 << led);

#else

    PORTA &= ~(1 << led);

#endif
}



/* =========================================================
   LED OFF
   ========================================================= */

void LED_Off(uint8_t led)
{
#if LED_ACTIVE_HIGH

    PORTA &= ~(1 << led);

#else

    PORTA |= (1 << led);

#endif
}



/* =========================================================
   LED 전체 OFF
   ========================================================= */

void LED_All_Off(void)
{
    uint8_t i;


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        LED_Off(i);
    }
}



/* =========================================================
   센서 상태 LED 표시
   ========================================================= */

void LED_Update(void)
{
    uint8_t i;


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        if (sensor_normalized[i] >= LINE_DETECT_THRESHOLD)
        {
            LED_On(i);
        }
        else
        {
            LED_Off(i);
        }
    }
}



/* =========================================================
   스위치 초기화
   ========================================================= */

void Switch_Init(void)
{
    DDRE &= ~(
        (1 << SW1_PIN) |
        (1 << SW2_PIN)
    );


    PORTE &= ~(
        (1 << SW1_PIN) |
        (1 << SW2_PIN)
    );
}



/* =========================================================
   버튼 눌림 확인
   ========================================================= */

uint8_t Switch_Pressed(uint8_t pin)
{
    if (!(PINE & (1 << pin)))
    {
        _delay_ms(20);


        if (!(PINE & (1 << pin)))
        {
            while (!(PINE & (1 << pin)))
            {
                ;
            }


            _delay_ms(20);


            return 1;
        }
    }


    return 0;
}



/* =========================================================
   PWM 초기화
   ========================================================= */

void PWM_Init(void)
{
    DDRB |=
        (1 << LEFT_PWM_PIN) |
        (1 << RIGHT_PWM_PIN);


    TCCR1A =
        (1 << COM1A1) |
        (1 << COM1B1) |
        (1 << WGM10);


    TCCR1B =
        (1 << CS10);


    OCR1A = 0;

    OCR1B = 0;
}



/* =========================================================
   모터 방향핀 초기화
   ========================================================= */

void Motor_Init(void)
{
    DDRB |=
        (1 << LEFT_IN1) |
        (1 << LEFT_IN2) |
        (1 << RIGHT_IN1) |
        (1 << RIGHT_IN2);


    PORTB &= ~(
        (1 << LEFT_IN1) |
        (1 << LEFT_IN2) |
        (1 << RIGHT_IN1) |
        (1 << RIGHT_IN2)
    );
}



/* =========================================================
   왼쪽 모터
   ========================================================= */

void Motor_Left(int16_t speed)
{
#if LEFT_MOTOR_REVERSED

    speed = -speed;

#endif


    if (speed > 255)
    {
        speed = 255;
    }


    if (speed < -255)
    {
        speed = -255;
    }


    if (speed > 0)
    {
        PORTB |=  (1 << LEFT_IN1);

        PORTB &= ~(1 << LEFT_IN2);


        OCR1A = (uint8_t)speed;
    }


    else if (speed < 0)
    {
        PORTB &= ~(1 << LEFT_IN1);

        PORTB |=  (1 << LEFT_IN2);


        OCR1A = (uint8_t)(-speed);
    }


    else
    {
        OCR1A = 0;


        PORTB &= ~(1 << LEFT_IN1);

        PORTB &= ~(1 << LEFT_IN2);
    }
}



/* =========================================================
   오른쪽 모터
   ========================================================= */

void Motor_Right(int16_t speed)
{
#if RIGHT_MOTOR_REVERSED

    speed = -speed;

#endif


    if (speed > 255)
    {
        speed = 255;
    }


    if (speed < -255)
    {
        speed = -255;
    }


    if (speed > 0)
    {
        PORTB |=  (1 << RIGHT_IN1);

        PORTB &= ~(1 << RIGHT_IN2);


        OCR1B = (uint8_t)speed;
    }


    else if (speed < 0)
    {
        PORTB &= ~(1 << RIGHT_IN1);

        PORTB |=  (1 << RIGHT_IN2);


        OCR1B = (uint8_t)(-speed);
    }


    else
    {
        OCR1B = 0;


        PORTB &= ~(1 << RIGHT_IN1);

        PORTB &= ~(1 << RIGHT_IN2);
    }
}



/* =========================================================
   모터 정지
   ========================================================= */

void Motor_Stop(void)
{
    Motor_Left(0);

    Motor_Right(0);
}



/* =========================================================
   직진
   ========================================================= */

void Motor_Forward(void)
{
    Motor_Left(FORWARD_SPEED);

    Motor_Right(FORWARD_SPEED);
}



/* =========================================================
   왼쪽 제자리 회전
   ========================================================= */

void Motor_Pivot_Left(void)
{
    Motor_Left(-PIVOT_SPEED);

    Motor_Right(PIVOT_SPEED);
}



/* =========================================================
   오른쪽 제자리 회전
   ========================================================= */

void Motor_Pivot_Right(void)
{
    Motor_Left(PIVOT_SPEED);

    Motor_Right(-PIVOT_SPEED);
}



/* =========================================================
   센서 정규화

   0    = 흰색
   1000 = 검은색
   ========================================================= */

void Normalize_Sensors(void)
{
    uint8_t i;

    uint16_t raw;

    uint16_t min_value;
    uint16_t max_value;

    uint16_t range;

    int32_t value;


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        raw = sensor_raw[i];

        min_value = sensor_min[i];

        max_value = sensor_max[i];


        if (max_value <= min_value)
        {
            sensor_normalized[i] = 0;

            continue;
        }


        range =
            max_value - min_value;


        if (range < MIN_CAL_RANGE)
        {
            sensor_normalized[i] = 0;

            continue;
        }


        if (raw < min_value)
        {
            raw = min_value;
        }


        if (raw > max_value)
        {
            raw = max_value;
        }



#if BLACK_IS_LOW

        value =
            ((int32_t)(max_value - raw) * 1000)
            / range;

#else

        value =
            ((int32_t)(raw - min_value) * 1000)
            / range;

#endif


        if (value < 0)
        {
            value = 0;
        }


        if (value > 1000)
        {
            value = 1000;
        }


        if (value < SENSOR_NOISE_FLOOR)
        {
            value = 0;
        }


        sensor_normalized[i] =
            (uint16_t)value;
    }
}



/* =========================================================
   캘리브레이션
   ========================================================= */

void Calibration(void)
{
    uint8_t i;

    uint16_t count;


    Motor_Stop();

    LED_All_Off();


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        sensor_min[i] = 1023;

        sensor_max[i] = 0;
    }


    for (count = 0; count < 1000; count++)
    {
        Read_All_Sensors();


        for (i = 0; i < SENSOR_COUNT; i++)
        {
            if (sensor_raw[i] < sensor_min[i])
            {
                sensor_min[i] =
                    sensor_raw[i];
            }


            if (sensor_raw[i] > sensor_max[i])
            {
                sensor_max[i] =
                    sensor_raw[i];
            }
        }


        Normalize_Sensors();

        LED_Update();


        _delay_ms(5);
    }


    Read_All_Sensors();

    Normalize_Sensors();

    LED_Update();


    calibration_done = 1;
}



/* =========================================================
   라인 위치 계산
   ========================================================= */

uint8_t Calculate_Line_Position(int16_t *position)
{
    uint8_t i;

    uint16_t max_sensor = 0;

    uint32_t total = 0;

    int32_t weighted_sum = 0;


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        uint16_t value;

        value = sensor_normalized[i];


        if (value > max_sensor)
        {
            max_sensor = value;
        }


        total += value;


        weighted_sum +=
            (int32_t)value *
            sensor_weight[i];
    }


    if (max_sensor < LINE_DETECT_THRESHOLD)
    {
        return 0;
    }


    if (total == 0)
    {
        return 0;
    }


    *position =
        (int16_t)(
            weighted_sum /
            (int32_t)total
        );


    return 1;
}



/* =========================================================
   라인 추종

   ★ 일반 주행에서도 가로선 검사를 항상 수행한다.
   ========================================================= */

void Line_Control(void)
{
    int16_t position;

    uint8_t line_found;

    uint8_t sensor_mask;

    uint8_t i;


    /* =====================================================
       1. ADC 읽기
       ===================================================== */

    Read_All_Sensors();


    /* =====================================================
       2. 정규화
       ===================================================== */

    Normalize_Sensors();


    /* =====================================================
       3. LED 표시
       ===================================================== */

    LED_Update();



    /* =====================================================
       4. 현재 IR 상태를 6비트 값으로 생성

       bit0 = IR1
       bit1 = IR2
       bit2 = IR3
       bit3 = IR4
       bit4 = IR5
       bit5 = IR6
       ===================================================== */

    sensor_mask = 0;


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        if (sensor_normalized[i] >= LINE_DETECT_THRESHOLD)
        {
            sensor_mask |= (1 << i);
        }
    }



    /* =====================================================
       5. 가로선 항상 검사

       첫 번째, 두 번째, 세 번째, 네 번째...
       가로선을 계속 검사한다.

       새로운 가로선이 검출되면
       HORIZON_COUNT가 증가한다.
       ===================================================== */

    HORIZON_COUNTER(sensor_mask);



    /* =====================================================
       6. 가중치 계산
       ===================================================== */

    line_found =
        Calculate_Line_Position(&position);



    /* =====================================================
       라인 발견
       ===================================================== */

    if (line_found)
    {
        /* -------------------------------------------------
           라인 왼쪽
           ------------------------------------------------- */

        if (position < -CENTER_DEADBAND)
        {
            last_direction = -1;


            Motor_Pivot_Left();
        }


        /* -------------------------------------------------
           라인 오른쪽
           ------------------------------------------------- */

        else if (position > CENTER_DEADBAND)
        {
            last_direction = 1;


            Motor_Pivot_Right();
        }


        /* -------------------------------------------------
           라인 중앙
           ------------------------------------------------- */

        else
        {
            if (position < -100)
            {
                last_direction = -1;
            }


            else if (position > 100)
            {
                last_direction = 1;
            }


            Motor_Forward();
        }
    }



    /* =====================================================
       라인 분실
       ===================================================== */

    else
    {
        if (last_direction < 0)
        {
            Motor_Pivot_Left();
        }


        else if (last_direction > 0)
        {
            Motor_Pivot_Right();
        }


        else
        {
            Motor_Stop();
        }
    }
}



/* =========================================================
   검은색을 인식하고 있는 IR 센서 개수
   ========================================================= */

uint8_t IR_COUNT(uint8_t mask)
{
    uint8_t count = 0;

    uint8_t i;


    for (i = 0; i < SENSOR_COUNT; i++)
    {
        if (mask & (1 << i))
        {
            count++;
        }
    }


    return count;
}



/* =========================================================
   가로선 판단 함수

   새로운 가로선을 발견할 때마다

   HORIZON_COUNT++

   return 1 = 새로운 가로선 발견
   return 0 = 발견하지 않음
   ========================================================= */

uint8_t HORIZON_COUNTER(uint8_t sensor_mask)
{
    uint8_t count;


    count = IR_COUNT(sensor_mask);



    /* =====================================================
       방금 가로선을 검출했다면

       같은 가로선에서 다시 카운트되지 않도록
       잠금 상태 유지
       ===================================================== */

    if (cross_latched)
    {
        /*
            다시 일반 라인 상태

            즉 검은색을 보는 센서가
            2개 이하가 되면

            현재 가로선을 빠져나왔다고 판단
        */
        if (count <= 2)
        {
            cross_latched = 0;
        }


        return 0;
    }



    /* =====================================================
       가로선 후보 검사 시작
       ===================================================== */

    if (!cross_checking)
    {
        /*
            동시에 4개 이상 검은색을 보면

            가로선 후보로 판단
        */
        if (count >= 4)
        {
            cross_checking = 1;


            cross_seen = 0;

            cross_timer = 0;


            left_seen = 0;

            right_seen = 0;

            wide_seen = 0;
        }


        else
        {
            return 0;
        }
    }



    /* =====================================================
       현재 검출된 IR 센서를 누적
       ===================================================== */

    cross_seen |= sensor_mask;



    /* =====================================================
       IR1 검출 기록
       ===================================================== */

    if (sensor_mask & (1 << 0))
    {
        left_seen = 1;
    }



    /* =====================================================
       IR6 검출 기록
       ===================================================== */

    if (sensor_mask & (1 << 5))
    {
        right_seen = 1;
    }



    /* =====================================================
       한 순간에 5개 이상 검출된 적이 있는지
       ===================================================== */

    if (count >= 5)
    {
        wide_seen = 1;
    }



    cross_timer++;



    /* =====================================================
       최종 가로선 판정

       1. IR1 ~ IR6 모두 누적 검출
       2. 왼쪽 끝 검출
       3. 오른쪽 끝 검출
       4. 한 순간에 5개 이상 검출
       ===================================================== */

    if (
        ((cross_seen & 0x3F) == 0x3F) &&
        left_seen &&
        right_seen &&
        wide_seen
       )
    {
        /*
            새로운 가로선 발견
        */
        HORIZON_COUNT++;


        /*
            같은 가로선 중복 검출 방지
        */
        cross_latched = 1;


        /*
            다음 검사를 위해 후보 상태 초기화
        */
        cross_checking = 0;

        cross_seen = 0;

        cross_timer = 0;

        left_seen = 0;

        right_seen = 0;

        wide_seen = 0;


        return 1;
    }



    /* =====================================================
       제한시간 초과

       가로선이 아닌 것으로 판단하고 초기화
       ===================================================== */

    if (cross_timer >= 30)
    {
        cross_checking = 0;

        cross_seen = 0;

        cross_timer = 0;

        left_seen = 0;

        right_seen = 0;

        wide_seen = 0;
    }


    return 0;
}



/* =========================================================
   ZONE 3

   첫 번째 가로선을 만나면 진입

   ZONE 3 안에서도
   가로선 검사를 계속 수행

   두 번째 가로선을 만나면
   HORIZON_COUNTER()가 1 반환

   → ZONE_3 종료

   → main으로 복귀
   ========================================================= */

void ZONE_3(void)
{
    int16_t position;

    uint8_t line_found;

    uint8_t sensor_mask;

    uint8_t i;


    while (1)
    {
        /* =========================================
           센서 읽기
           ========================================= */

        Read_All_Sensors();


        Normalize_Sensors();


        LED_Update();



        /* =========================================
           현재 IR 상태를 6비트로 변환
           ========================================= */

        sensor_mask = 0;


        for (i = 0; i < SENSOR_COUNT; i++)
        {
            if (sensor_normalized[i] >= LINE_DETECT_THRESHOLD)
            {
                sensor_mask |= (1 << i);
            }
        }



        /* =========================================
           ZONE 3에서도 가로선 검사

           첫 번째 가로선을 발견한 직후에는
           cross_latched = 1이므로

           같은 첫 번째 가로선은
           다시 카운트되지 않는다.

           첫 번째 가로선을 빠져나온 뒤
           다음 가로선을 만나면

           HORIZON_COUNT

           1 → 2

           가 되고 return 1
           ========================================= */

        if (HORIZON_COUNTER(sensor_mask))
        {
            /*
                두 번째 가로선이면
                ZONE 3 종료
            */
            if (HORIZON_COUNT == 3)
            {
                return;
            }
        }



        /* =========================================
           ZONE 3 라인 위치 계산
           ========================================= */

        line_found =
            Calculate_Line_Position(&position);



        /* =========================================
           ZONE 3 주행
           ========================================= */

        if (line_found)
        {
            /*
                라인이 왼쪽이면
                오른쪽으로 회전
            */
            if (position < -CENTER_DEADBAND)
            {
                last_direction = -1;


                Motor_Pivot_Right();
            }


            /*
                라인이 오른쪽이면
                왼쪽으로 회전
            */
            else if (position > CENTER_DEADBAND)
            {
                last_direction = 1;


                Motor_Pivot_Left();
            }


            /*
                라인 중앙
            */
            else
            {
                Motor_Forward();
            }
        }


        else
        {
            Motor_Forward();
        }
    }
}



/* =========================================================
   MAIN
   ========================================================= */

int main(void)
{
    /*
        PF4 ~ PF6 ADC 사용
    */
    JTAG_Disable();


    ADC_Init();

    Motor_Init();

    PWM_Init();

    Switch_Init();

    LED_Init();



    /* =====================================================
       초기 상태
       ===================================================== */

    Motor_Stop();

    LED_All_Off();



    while (1)
    {
        /* =================================================
           주행 시작 전
           ================================================= */

        if (!running)
        {
            /* ---------------------------------------------
               SW1 : 캘리브레이션
               --------------------------------------------- */

            if (Switch_Pressed(SW1_PIN))
            {
                Calibration();
            }



            /*
                캘리브레이션 완료 후
                센서 상태 LED 표시
            */
            if (calibration_done)
            {
                Read_All_Sensors();

                Normalize_Sensors();

                LED_Update();
            }



            /* ---------------------------------------------
               SW2 : 주행 시작
               --------------------------------------------- */

            if (calibration_done)
            {
                if (Switch_Pressed(SW2_PIN))
                {
                    running = 1;


                    last_direction = 0;


                    /*
                        출발 시 가로선 카운트 초기화
                    */
                    HORIZON_COUNT = 0;


                    cross_seen = 0;

                    cross_checking = 0;

                    cross_timer = 0;

                    left_seen = 0;

                    right_seen = 0;

                    wide_seen = 0;

                    cross_latched = 0;
                }
            }



            if (!running)
            {
                Motor_Stop();
            }
        }



        /* =================================================
           주행 시작
           ================================================= */

        else
        {
            /* =============================================
               일반 라인트레이싱

               Line_Control 내부에서
               가로선 검사도 항상 실행됨
               ============================================= */

            Line_Control();



            /* =============================================
               지금까지 발견한 가로선 개수에 따라
               필요한 동작 수행
               ============================================= */

            switch (HORIZON_COUNT)
            {
                /* =========================================
                   첫 번째 가로선

                   ZONE 3 진입
                   ========================================= */

                case 2:

                    ZONE_3();

                    break;



                /* =========================================
                   두 번째 가로선

                   ZONE_3은 이미 종료된 상태

                   이후에는 일반 Line_Control()
                   주행으로 계속 진행
                   ========================================= */

                case 4:

                    break;



                /* =========================================
                   나중에 추가 가능

                   세 번째 가로선

                case 3:

                    원하는_동작();

                    break;


                   네 번째 가로선

                case 4:

                    원하는_동작();

                    break;

                   ========================================= */


                default:

                    break;
            }
        }
    }


    return 0;
}

