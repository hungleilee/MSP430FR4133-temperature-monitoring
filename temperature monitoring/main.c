#include <msp430.h>
#include <stdlib.h>
#define CALADC_15V_30C  *((unsigned int *)0x1A1A)                 // Temperature Sensor Calibration-30 C                                                                  // See device datasheet for TLV table memory mapping
#define CALADC_15V_85C  *((unsigned int *)0x1A1C)                 // Temperature Sensor Calibration-85 C

#define ERROR_VALUE 100
#define CORRUPTION_THRESHOLD 50

#define NUM_READINGS 10
float backupTemperatureReadings[NUM_READINGS];
float temperatureReadings[NUM_READINGS];
int readIndex = 0;
float total = 0;
float average = 0;

float temp;
float IntDegF;
float IntDegC;
#define pos1 4                                                 // Digit A1 - L4
#define pos2 6                                                 // Digit A2 - L6
#define pos3 8                                                 // Digit A3 - L8

typedef int bool;
#define true 1
#define false 0

// Digit pattern for LCD (0-9)
const char digit[10] = {
    0xFC, // "0"
    0x60, // "1"
    0xDB, // "2"
    0xF3, // "3"
    0x67, // "4"
    0xB7, // "5"
    0xBF, // "6"
    0xE4, // "7"
    0xFF, // "8"
    0xF7  // "9"
};

void setup()
{
    // put your setup code here, to run once:
    // Configure LCD pins
    SYSCFG2 |= LCDPCTL;                                 // R13/R23/R33/LCDCAP0/LCDCAP1 pins selected

    LCDPCTL0 = 0xFFFF;
    LCDPCTL1 = 0x07FF;
    LCDPCTL2 = 0x00F0;                                  // L0~L26 & L36~L39 pins selected

    LCDCTL0 = LCDSSEL_0 | LCDDIV_7;                     // flcd ref freq is xtclk

    // LCD Operation - Mode 3, internal 3.08v, charge pump 256Hz
    LCDVCTL = LCDCPEN | LCDREFEN | VLCD_6 | (LCDCPFSEL0 | LCDCPFSEL1 | LCDCPFSEL2 | LCDCPFSEL3);

    LCDMEMCTL |= LCDCLRM;                               // Clear LCD memory

    LCDCSSEL0 = 0x000F;                                 // Configure COMs and SEGs
    LCDCSSEL1 = 0x0000;                                 // L0, L1, L2, L3: COM pins
    LCDCSSEL2 = 0x0000;

    LCDM0 = 0x21;                                       // L0 = COM0, L1 = COM1
    LCDM1 = 0x84;                                       // L2 = COM2, L3 = COM3

    LCDCTL0 |= LCD4MUX | LCDON;                         // Turn on LCD, 4-mux selected (LCD4MUX also includes LCDSON)

    //Serial.begin(9600) ; //not support
    //Serial.println(12) ;
    WDTCTL = WDTPW | WDTHOLD;                                     // Stop WDT

    PM5CTL0 &= ~LOCKLPM5;                                         // Disable high-impedance mode

    TA0CCTL0 |= CCIE;                                             // TACCR0 interrupt enabled
    TA0CCR0 = 65535;
    TA0CTL = TASSEL__ACLK | MC__UP;                               // ACLK, UP mode

    // Configure ADC - Pulse sample mode; ADCSC trigger
    ADCCTL0 |= ADCSHT_8 | ADCON;                                  // ADC ON,temperature sample period>30us
    ADCCTL1 |= ADCSHP;                                            // s/w trig, single ch/conv, MODOSC
    ADCCTL2 |= ADCRES;                                            // 10-bit conversion results
    ADCMCTL0 |= ADCSREF_1 | ADCINCH_12;                           // ADC input ch A12 => temp sense
    ADCIE |=ADCIE0;                                               // Enable the Interrupt request for a completed ADC_B conversion

    // Configure reference
    PMMCTL0_H = PMMPW_H;                                          // Unlock the PMM registers
    PMMCTL2 |= INTREFEN | TSENSOREN;                              // Enable internal reference and temperature sensor

    __delay_cycles(400);                                          // Delay for reference settling

//    __bis_SR_register(LPM0_bits | GIE);                           // LPM3 with interrupts enabled
//    __no_operation();                                             // Only for debugger
}

void displayTemperature(float avgTemp) {
    // Convert float to int for display purposes

    int avgTempInt = (int)avgTemp;

    // Display average temperature on the LCD
    LCDMEM[pos1] = digit[avgTempInt / 10]; //Tens place for average
    LCDMEM[pos2] = digit[avgTempInt % 10]; //Ones place for average


    //LCDMEM[pos3] = digit[1];//test
}

void loop()
{

    ADCCTL0 |= ADCENC | ADCSC;
    temp = ADCMEM0;
    IntDegC = (temp-CALADC_15V_30C)*(85-30)/(CALADC_15V_85C-CALADC_15V_30C)+30;
    IntDegF = 9*IntDegC/5+32;

    total -= temperatureReadings[readIndex];
    temperatureReadings[readIndex] = IntDegC;

    //Part 2: Demonstration of Data Inconsistency and Integrity
    int randomNumber = rand() % 20 + 1;//5% error rate
    if (randomNumber == 1) {
        temperatureReadings[readIndex] += ERROR_VALUE;
    }

    //Part 3: Checkpoint Mechanism Implementation
    if (detectDataCorruption()) {
            restoreFromBackup();
        } else {
            backupReadings();
        }

    //update
    total += temperatureReadings[readIndex];
    readIndex = (readIndex + 1) % NUM_READINGS;

    average = total / NUM_READINGS;

    // Update the display with current and average temperature
    displayTemperature( average);

    // Insert a delay for display update rate control
    __delay_cycles(500000);

}


bool detectDataCorruption() {
    return temperatureReadings[readIndex] > CORRUPTION_THRESHOLD;
}
void backupReadings() {
    int i;
    for (i = 0; i < NUM_READINGS; i++) {
        backupTemperatureReadings[i] = temperatureReadings[i];
    }
}
void restoreFromBackup() {
    int i;
    for (i = 0; i < NUM_READINGS; i++) {
        temperatureReadings[i] = backupTemperatureReadings[i];
    }
}

// ADC interrupt service routine
#pragma vector=ADC_VECTOR
__interrupt void ADC_ISR(void)
{
    temp = ADCMEM0;
    // Temperature in Celsius
    IntDegC = (temp-CALADC_15V_30C)*(85-30)/(CALADC_15V_85C-CALADC_15V_30C)+30;

    // Temperature in Fahrenheit
    IntDegF = 9*IntDegC/5+32;

}

// Timer A0 interrupt service routine

#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer_A (void)

{
    ADCCTL0 |= ADCENC | ADCSC;                                    // Sampling and conversion start
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;
    LCDMEM[pos1] = digit[0]; // Initialize all segments to show 0
    LCDMEM[pos2] = digit[0];
    //LCDMEM[pos3] = digit[0];//test
    setup();

    while (1) {
        loop();
    }
//    __bis_SR_register(LPM3_bits | GIE);
//    __no_operation(); // For debugger
    // No need to return from main in embedded programs
}
