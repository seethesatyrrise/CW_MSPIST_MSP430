//
//		Программа для контроля работы стабилитрона (диода Зенера).
//	Сигнал подается на ЦАП микроконтроллера, далее через операционный усилитель подается на АЦП. В цепь включен 
//	стабилитрон для ограничения напряжения, выходящего на АЦП. По нажатию на кнопку происходит запуск преобразований.
//	На ЦАП последовательно подаются напряжения от 0 до 2,5В с шагом в 50мВ. По окончанию преобразования сигнала
//	ЦАП посылается команда АЦП начать преобразование. Для каждого напряжения преобразование в АЦП производится 9 раз
//	для получения медианного значения. Производится проверка полученного значения на превышение заданного предела.
//	В случае превышения преобразования приостанавливаются и на экран выводится слово FAIL. Если ни одно из полученных
//	значений не вышло за пределы, то считаем, что стабилитрон правильно работает, на экран выводится слово PASS. 
//	Повторное нажание кнопки очищает экран.
//

#include <msp430xG46x.h>
#include <stdio.h>
#include "LCD.h"
#define DAC_V_REF 2500
#define V_LIMIT 4030
#define ADC_CYCLES_AMOUNT 9
#define ADC_MEDIAN ADC_CYCLES_AMOUNT/2
#define DAC_RESOLUTION 4096.

inline void setupADC();
inline void setupDAC();
inline void setupPorts();
volatile int DAC12_0V;
volatile int DAC_pass = 0;
volatile int DAC_V_step = 50;
volatile int read_iter = 0;
volatile int i = 0;
volatile int j = 0;
volatile int k = 0;
volatile int Vr;
volatile int vout[10];
volatile bool device_is_ready = true;
 
int main(void) {
    WDTCTL = WDTPW | WDTHOLD; // Stop watchdog timer
    setupADC();
    setupDAC();
    setupPorts();
 
    __enable_interrupt();
 
    initLCD_A();
    clrLCD();
 
    __bis_SR_register(LPM0_bits + GIE);
}
 
void setupDAC(void) {
 // dac0 DAC12.0
    DAC12_0CTL = DAC12IR
            | DAC12AMP_5
            | DAC12ENC
            | DAC12IE
            | DAC12LSEL0;
			
    printf("dac on\n");
}
 
void setupADC(void) {
    ADC12CTL0 = REF2_5V // set reference voltage (for DAC)
            | REFON // enable reference voltage generator (for DAC)
            | ADC12ON // enable adc
//            | MSC // enable multi sample conversion
            | SHT0_12; // 1024 cycles
    ADC12CTL1 |= CONSEQ_0
            | SHP // multichannel
            | CSTARTADD0;
			
    // A5
    P6SEL |= BIT5;
    //P6DIR = 0x00;
    ADC12MCTL1 |= INCH_5;
	
    printf("adc on\n");
	
    ADC12CTL0 |= ENC;
    ADC12IE = 0xFFFF;
}
 
void setupPorts() {
    // init button
    P1SEL &= ~BIT0; // enable GPIO in P1.0
    P1IE |= BIT0; // enable interrupt from P1.0
    P1IES |= BIT0; // select interrupt-edge
    P1DIR &= ~BIT0; // set P1.0 to input
    // init all ports
    P2SEL &= ~BIT0;
    P3SEL &= ~BIT0;
    P4SEL &= ~BIT0;
    P7SEL &= ~BIT0;
    P8SEL &= ~BIT0;
    P9SEL &= ~BIT0;
    P10SEL &= ~BIT0;
    PASEL &= ~BIT0;
    PBSEL &= ~BIT0;
}
 
#pragma vector=DAC12_VECTOR
__interrupt void DAC12ISR(void) {
    //printf("dac interrupt\n");
    DAC12_0CTL &= ~DAC12IFG;
    ADC12CTL0 |= ADC12SC;								//tell ADC to work
}
 
#pragma vector=ADC12_VECTOR
__interrupt void ADC12ISR(void) {						//read ADC 9 times to take the median voltage
    //printf("adc interrupt, %d\n", read_iter);			//and check if it's below the limit
    if(read_iter < ADC_CYCLES_AMOUNT) {		 			//then display the result if DAC reached it's max V
        vout[read_iter] = ADC12MEM1;
        //printf("%d\t", vout[DAC_pass]);
        read_iter++;
        ADC12CTL0 |= ADC12SC;
    } else {
        vout[ADC_CYCLES_AMOUNT] = ADC12MEM1;
        read_iter = 0;
		
        //printf("\n");
        for(i = ADC_CYCLES_AMOUNT; i > 0; i--) {			//sort results
            for(j = 0; j < i; j++) {
                if(vout[j] > vout[j + 1]) {
                   k = vout[j];
                   vout[j] = vout[j + 1];
                   vout[j + 1] = k;
                }
            }
        }
		
        printf("DAC_pass: %d, Vin: %d, Vout: %d\n", DAC_pass, DAC12_0V, vout[ADC_MEDIAN]);
		
        if(abs(vout[ADC_MEDIAN]) < V_LIMIT) {
            if(DAC12_0V < DAC_V_REF) {
                DAC_pass++;
                DAC12_0V += DAC_V_step;
                DAC12_0DAT = DAC12_0V * (DAC_RESOLUTION / DAC_V_REF);
            } else {
				printf("pass\n");
                clrLCD();
                displayPass();
				
                __bis_SR_register_on_exit(LPM0_bits);
            }
        } else {
            printf("fail\n");
			
            clrLCD;
            displayFail();
        }
    }
}
 
#pragma vector = PORT1_VECTOR			//interruption from button
__interrupt void PORT1_ISR(void) {
    P1IE &= ~BIT0;
	
    if (!(P1IN & BIT0)) {
        if(device_is_ready) {
            printf("btn\n");

            read_iter = 0;
            DAC_pass = 0;
            displayWait();
            DAC12_0V = 0;
            DAC12_0DAT = DAC12_0V * (DAC_RESOLUTION / DAC_V_REF);
            device_is_ready = false;
        } else {
            clrLCD();
            device_is_ready = true;
            __bis_SR_register_on_exit(LPM0_bits);
        }
    }
	
    P1IE |= BIT0;
    P1IFG &= ~BIT0;
}
