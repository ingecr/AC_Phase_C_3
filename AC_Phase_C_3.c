/*
 * File:   AC_Phase_C_3.c
 * Author: juand
 *
 * Created on May 27, 2019, 4:02 PM
 */


#include <xc.h>
#include "pic18f45k50.h"

#pragma config MCLRE = OFF      // MCLR Pin Enable bit (Disabled)
#pragma config FOSC = HSH       // Oscillator Selection (HS oscillator, high power 16MHz to 25MHz)

#define LEDFunction LATD4  //Define una variable a un pin de salida
#define LEDZEROCross LATD5 //Define una variable a un pin de salida
#define LEDTRIAC LATD6     //Define una variable a un pin de salida
#define LEDADC LATD7      //Define una variable a un pin de salida

unsigned int Timer1 =0;   //Variable en la que cuenta el tiempo entre cruces por 0
unsigned int Ainput =0;   //Variable en la que se guarda el valor leido por el ADC

unsigned int Pic_ADDR = 0b00011011;         //0x1B 27d
unsigned int trash = 0;
unsigned int REF = 50;

/*Funcion de inicializacion de puertos p.107*/
void Port_init (void)
{
    TRISD=0b00001111; //PORT D4, D5, D6, D7 como salida
    
    TRISA0=1;         //PORT A0 como entrada POT
    
    TRISB2=1;         //PORT B2 como entrada Zero Cross Interrupt
    ANSB2=0;          //Analog pin disable 
    
    //LEDs init
    LEDFunction=1;    //Inicializa LED apagado
    LEDZEROCross=1;   //Inicializa LED apagado
    LEDTRIAC=1;       //Inicializa LED apagado
    LEDADC=1;         //Inicializa LED apagado
    
    //I2C Pins
    TRISB0=1;
    ANSB0=0;
    TRISB1=1;
    ANSB1=0;
    
}

/*Funcion de inicializacion de interrupciones p.92*/
void ISR_init (void)
{
    IPEN=1; //Habilitamos niveles de prioridad
    GIEH=1; //Habilitador global
    
    //Timer 0 
    TMR0IE = 1; // Habilitador de interrupcion TMR0
    TMR0IF = 0; //Bandera de overload TMR0
    TMR0IP = 1; // Habilitador de interrupcion TMR0
    
    //Analog POT
    ADIE=1; // Habilitador de interrupcion ADC
    ADIP=1; // Habilitador de interrupcion ADC
    ADIF=0;
    
    //Pin interrupt zero cross
    INT2IE=1; // Habilitador de interrupcion PORTB2
    INT2IP=1; //INT2 External interrupt priority bit
    INT2IF=0;
    INTCON2bits.INTEDG2=0;
    
    //I2C
    SSPIE = 1;
    SSPIP = 1;
    SSPIF = 0;
    
    
}

/*Funcion de inicializacion de Timer0 p.133*/
void Timer0_init (void)
{
    T0CON=0b01000000; // Timer0 diseñado a 23us
}

/*Funcion de inicializacion de ADC p.243*/
void ADC_init (void) 
{
    ADCON0=0b00000001; //Iniciacion ADC (Single-shot,single channel, group A)
    ADCON1=0;          //(Internal reference VDD y Vss, FIFO disabled, Buffer adress 0)
    ADCON2=0b00010001; // (Left justified, 4TAD, )
   
}

void I2C_init(void)
{
    SSP1STATbits.SMP = 0;
    SSP1STATbits.CKE = 0;
    SSP1CON1 = 0b00110110;
    SSP1CON2 = 0;
    SSP1ADD = Pic_ADDR<<1;
    
}

/*Funcion de comparador de voltaje (0V a 2.5V y 2.5V a 5V) encendiendo un LED indicador*/
void LED_Comp_ADC (void)
{
    if (REF>=125){
            LEDADC=0; //Enciendo el LED
        }
        else{
            LEDADC=1; //Apago el LED
        }
}

/*Funcion de señal de control, dependiendo del porcentaje que quieres que se 
mantenga encendido se encendera y se apagara antes de que llegue el proximo cruce por 0 */
void ACPhaseControl (void)
{
        if (REF<=2){
            REF=1;
        }
        if (Timer1==REF){ //Tiempo en el que quieres encender la señal
            LEDTRIAC=0;
        }
       
        if (Timer1>=250){ //Tiempo de apagar la señal, antes del curce por 0 //490 //240
             LEDTRIAC=1;
             Timer1=0;    //Se reinicia la cuenta del tiempo
             TMR0ON=0;    //Se apaga el Timer0
        }
}

void __interrupt() high(void) { //Alta prioridad
    if (SSPIF == 1 && SSPIE == 1) 
    {       
        if(SSPCONbits.SSPOV == 1 || SSPCONbits.WCOL == 1)
        {
            trash = SSPBUF;
            SSPCONbits.SSPOV = 0;
            SSPCONbits.WCOL = 0;
            SSPCONbits.CKP = 1;
        }
        
        if (SSPSTATbits.D_NOT_A == 0){
        
            trash = SSPBUF;
            
            SSPCONbits.CKP = 1;

        }else{
        
            if(BF == 1 ){
        
            REF = SSPBUF;
            SSPCONbits.CKP = 1;
            }
        }
        
        SSPIF = 0;
    }
    
    if ((INT2IF==1)&&(INT2IE==1)) {  // Interrupcion por cruce por 0
        INT2IF=0;
        LEDZEROCross=~LEDZEROCross;  //Led indicador de introduccion a la interrupcion de pin
        TMR0ON=1;
        GO_DONE=1;                   //Inicia conversion de ADC
     }
    
    if ((TMR0IE==1)&&(TMR0IF==1)){    // Interrupcion de cuentas de Timer0 cada 23us
        TMR0IF=0;
        TMR0=204; //250 //125
        Timer1++;                     //Variable de tiempo utilizada para la funcion de control
        LED_Comp_ADC ();              //Funcion de comparacion de voltaje
        LEDFunction=~LEDFunction;
        ACPhaseControl();             //Funcion de Señal de control
        
     }
    
if ((ADIF==1)&&(ADIE==1)) {  //       //Interrupcion de finalizado de conversion 
        ADIF=0;
        REF=ADRESH;                //Guarda el valor obtenido de la conversion en una variable
    }
}

void main(void) 
{
    
    Port_init();
    ISR_init();
    Timer0_init();
    I2C_init();
    ADC_init();
    
    while(1)
    {
        
    }
    
    
}
