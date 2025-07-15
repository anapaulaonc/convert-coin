#include <msp430.h>
#include <stdint.h>
#include <stdio.h>

#define LCD_ADDR 0x27
#define LCD_RS  BIT0
#define LCD_RW  BIT1
#define LCD_EN  BIT2
#define LCD_BL  BIT3

uint8_t i2cSend(uint8_t addr, uint8_t data) {
    while (UCB0CTL1 & UCTXSTP);
    UCB0I2CSA = addr;
    UCB0CTL1 |= UCTR | UCTXSTT;
    while (!(UCB0IFG & UCTXIFG));
    UCB0TXBUF = data;
    while (!(UCB0IFG & UCTXIFG));
    UCB0CTL1 |= UCTXSTP;
    while (UCB0CTL1 & UCTXSTP);
    return 1;
}

void lcdWriteNibble(uint8_t nibble, uint8_t isChar) {
    uint8_t data = (nibble & 0xF0);
    data |= LCD_BL;
    if (isChar) data |= LCD_RS;

    i2cSend(LCD_ADDR, data);
    i2cSend(LCD_ADDR, data | LCD_EN);
    __delay_cycles(2000);
    i2cSend(LCD_ADDR, data);
}

void lcdWriteByte(uint8_t byte, uint8_t isChar) {
    lcdWriteNibble(byte & 0xF0, isChar);
    lcdWriteNibble((byte << 4) & 0xF0, isChar);
}

void lcdInit() {
    __delay_cycles(50000);

    lcdWriteNibble(0x30, 0);
    __delay_cycles(5000);
    lcdWriteNibble(0x30, 0);
    __delay_cycles(5000);
    lcdWriteNibble(0x30, 0);
    __delay_cycles(5000);
    lcdWriteNibble(0x20, 0);

    lcdWriteByte(0x28, 0);
    lcdWriteByte(0x0C, 0);
    lcdWriteByte(0x06, 0);
    lcdWriteByte(0x01, 0);
    __delay_cycles(5000);
}

void lcdWrite(char *str) {
    while (*str) {
        if (*str == '\n') {
            lcdWriteByte(0xC0, 0);
        } else {
            lcdWriteByte(*str, 1);
        }
        str++;
    }
}

void lcdClear() {
    lcdWriteByte(0x01, 0);
    __delay_cycles(5000);
}

void i2cConfig(){
    UCB0CTL1 = UCSWRST;
    UCB0CTL0 = UCMST | UCMODE_3 | UCSYNC;
    UCB0CTL1 |= UCSSEL__SMCLK;
    UCB0BRW = 100;
    P3SEL |= BIT0 | BIT1;
    P3DIR &= ~(BIT0 | BIT1);
    UCB0CTL1 &= ~UCSWRST;
}

void adcConfig() {
    P6SEL |= BIT0 | BIT1;
    ADC12CTL0 = ADC12SHT0_3 | ADC12ON;
    ADC12CTL1 = ADC12SHP;
    ADC12CTL2 = ADC12RES_0;
}

void buttonConfig() {
    P2DIR &= ~BIT5;
    P2REN |= BIT5;
    P2OUT |= BIT5;
}

uint8_t readADC(uint8_t channel) {
    ADC12CTL0 &= ~ADC12ENC;
    ADC12MCTL0 = channel;
    ADC12CTL0 |= ADC12ENC;
    ADC12CTL0 |= ADC12SC;
    while (ADC12CTL1 & ADC12BUSY);
    return ADC12MEM0 & 0xFF;
}

void showCurrency(int coin, char* prefix) {
    lcdWrite(prefix);
    switch(coin){
        case 1:
            lcdWrite("BRL (Real)\n");
            break;
        case 2:
            lcdWrite("USD (Dolar)\n");
            break;
        case 3:
            lcdWrite("EUR (Euro)\n");
            break;
        case 4:
            lcdWrite("BTC (Bitcoin)\n");
            break;
        case 5:
            lcdWrite("JPY (Yen)\n");
            break;
    }
}

char* getCurrencySymbol(int coin) {
    switch(coin) {
        case 1: return "BRL";
        case 2: return "USD";
        case 3: return "EUR";
        case 4: return "BTC";
        case 5: return "JPY";
        default: return "???";
    }
}

long convertToBase(int fromCoin, long amountCents) {
    switch(fromCoin) {
        case 1: // BRL
            return amountCents;
        case 2: // USD
            return (amountCents * 500L) / 100L;
        case 3: // EUR
            return (amountCents * 555L) / 100L;
        case 4: // BTC (satoshis para BRL)
            return amountCents * 20000L;
        case 5: // JPY
            return (amountCents * 100L) / 3000L;
        default:
            return amountCents;
    }
}

void convertFromBase(int toCoin, long baseCents, int* intPart, int* decPart) {
    long result;
    
    switch(toCoin) {
        case 1: // BRL
            result = baseCents;
            break;
        case 2: // USD
            result = (baseCents * 100L) / 500L;
            break;
        case 3: // EUR
            result = (baseCents * 100L) / 555L;
            break;
        case 4: // BTC
            result = baseCents / 20000L;
            if (result == 0 && baseCents > 0) result = 1;
            *intPart = (int)result;
            *decPart = 0;
            return;
        case 5: // JPY
            result = (baseCents * 3000L) / 100L;
            break;
        default:
            result = baseCents;
    }
    
    *intPart = (int)(result / 100L);
    *decPart = (int)(result % 100L);
}

void showResult(int fromCoin, int toCoin, long amountCents, int intPart, int decPart) {
    char buffer[32];
    char* fromSymbol = getCurrencySymbol(fromCoin);
    char* toSymbol = getCurrencySymbol(toCoin);
    
    lcdClear();
    
    int fromInt = (int)(amountCents / 100L);
    int fromDec = (int)(amountCents % 100L);
    
    if (fromCoin == 4) {
        sprintf(buffer, "%s %d = \n", fromSymbol, fromInt);
    } else {
        if (fromDec < 10) {
            sprintf(buffer, "%s %d.0%d = \n", fromSymbol, fromInt, fromDec);
        } else {
            sprintf(buffer, "%s %d.%d = \n", fromSymbol, fromInt, fromDec);
        }
    }
    lcdWrite(buffer);
    
    if (toCoin == 4) {
        sprintf(buffer, "%s %d\n", toSymbol, intPart);
    } else {
        if (decPart < 10) {
            sprintf(buffer, "%s %d.0%d\n", toSymbol, intPart, decPart);
        } else {
            sprintf(buffer, "%s %d.%d\n", toSymbol, intPart, decPart);
        }
    }
    lcdWrite(buffer);
}

void main(void) {
    WDTCTL = WDTPW | WDTHOLD;

    i2cConfig();
    adcConfig();
    buttonConfig();
    lcdInit();

    unsigned int fromCoin = 1;
    unsigned int toCoin = 2;
    unsigned int fase = 0;

    unsigned int lastXDir = 0;
    unsigned int lastYDir = 0;
    unsigned int lastButtonState = 1;

    static uint8_t valor[5] = {0, 0, 0, 0, 0};
    static uint8_t cursorPos = 0;

    int convertedInt = 0;
    int convertedDec = 0;

    lcdClear();
    lcdWrite("COTACAO LIVE\n");
   
    __delay_cycles(3000000);
    
    lcdClear();
    lcdWrite("5 Moedas\n");
    lcdWrite("Aperte botao...");
    
    while (1) {
        unsigned int buttonState = P2IN & BIT5;
        if (buttonState == 0 && lastButtonState != 0) {
            break;
        }
        lastButtonState = buttonState;
        __delay_cycles(50000);
    }
    
    lcdClear();
    showCurrency(fromCoin, "De: ");

    while (1) {
        unsigned int xValue = readADC(ADC12INCH_0);
        unsigned int yValue = readADC(ADC12INCH_1);

        uint8_t precisaAtualizarLCD = 0;

        if (fase == 0) {
            if (xValue > 110 && xValue < 140) {
                lastXDir = 0;
            } else if (xValue >= 180 && lastXDir != 1) {
                if (fromCoin < 5) fromCoin++;
                lcdClear();
                showCurrency(fromCoin, "De: ");
                lastXDir = 1;
                __delay_cycles(200000);
            } else if (xValue <= 50 && lastXDir != 2) {
                if (fromCoin > 1) fromCoin--;
                lcdClear();
                showCurrency(fromCoin, "De: ");
                lastXDir = 2;
                __delay_cycles(200000);
            }
        }

        if (fase == 1) {
            if (xValue > 110 && xValue < 140) {
                lastXDir = 0;
            } else if (xValue >= 180 && lastXDir != 1) {
                if (toCoin < 5) toCoin++;
                lcdClear();
                showCurrency(toCoin, "Para: ");
                lastXDir = 1;
                __delay_cycles(200000);
            } else if (xValue <= 50 && lastXDir != 2) {
                if (toCoin > 1) toCoin--;
                lcdClear();
                showCurrency(toCoin, "Para: ");
                lastXDir = 2;
                __delay_cycles(200000);
            }
        }

        if (fase == 2) {
            if (xValue > 110 && xValue < 140) {
                lastXDir = 0;
            } else if (xValue >= 180 && lastXDir != 1) {
                if (cursorPos < 4) cursorPos++;
                lastXDir = 1;
                precisaAtualizarLCD = 1;
                __delay_cycles(200000);
            } else if (xValue <= 50 && lastXDir != 2) {
                if (cursorPos > 0) cursorPos--;
                lastXDir = 2;
                precisaAtualizarLCD = 1;
                __delay_cycles(200000);
            }

            if (yValue > 110 && yValue < 140) {
                lastYDir = 0;
            } else if (yValue >= 180 && lastYDir != 1) {
                if (valor[cursorPos] > 0) valor[cursorPos]--;
                lastYDir = 1;
                precisaAtualizarLCD = 1;
                __delay_cycles(200000);
            } else if (yValue <= 50 && lastYDir != 2) {
                if (valor[cursorPos] < 9) valor[cursorPos]++;
                lastYDir = 2;
                precisaAtualizarLCD = 1;
                __delay_cycles(200000);
            }

            if (precisaAtualizarLCD) {
                lcdClear();
                char buffer[16];
                char* symbol = getCurrencySymbol(fromCoin);
                
                if (fromCoin == 4) {
                    sprintf(buffer, "%d%d%d%d%d %s", valor[0], valor[1], valor[2], valor[3], valor[4], symbol);
                } else {
                    sprintf(buffer, "%d%d%d.%d%d %s", valor[0], valor[1], valor[2], valor[3], valor[4], symbol);
                }
                lcdWrite(buffer);

                char cursorLine[17] = "                ";
                if (fromCoin == 4) {
                    cursorLine[cursorPos] = '^';
                } else {
                    if (cursorPos < 3)
                        cursorLine[cursorPos] = '^';
                    else
                        cursorLine[cursorPos + 1] = '^';
                }

                lcdWriteByte(0xC0, 0);
                lcdWrite(cursorLine);

                precisaAtualizarLCD = 0;
            }
        }

        unsigned int buttonState = P2IN & BIT5;
        if (buttonState == 0 && lastButtonState != 0) {
            if (fase == 0) {
                fase = 1;
                if (toCoin == fromCoin) toCoin = (fromCoin == 1) ? 2 : 1;
                lcdClear();
                showCurrency(toCoin, "Para: ");
                
            } else if (fase == 1) {
                fase = 2;
                valor[0] = valor[1] = valor[2] = valor[3] = valor[4] = 0;
                cursorPos = 0;
                lcdClear();
                char buffer[16];
                char* symbol = getCurrencySymbol(fromCoin);
                
                if (fromCoin == 4) {
                    sprintf(buffer, "%d%d%d%d%d %s", valor[0], valor[1], valor[2], valor[3], valor[4], symbol);
                } else {
                    sprintf(buffer, "%d%d%d.%d%d %s", valor[0], valor[1], valor[2], valor[3], valor[4], symbol);
                }
                lcdWrite(buffer);
                
                char cursorLine[17] = "^               ";
                lcdWriteByte(0xC0, 0);
                lcdWrite(cursorLine);
                
            } else if (fase == 2) {
                long amountCents;
                if (fromCoin == 4) {
                    amountCents = (long)valor[0]*10000L + (long)valor[1]*1000L + (long)valor[2]*100L + (long)valor[3]*10L + (long)valor[4];
                } else {
                    amountCents = (long)valor[0]*10000L + (long)valor[1]*1000L + (long)valor[2]*100L + (long)valor[3]*10L + (long)valor[4];
                }
                
                long baseCents = convertToBase(fromCoin, amountCents);
                convertFromBase(toCoin, baseCents, &convertedInt, &convertedDec);
                showResult(fromCoin, toCoin, amountCents, convertedInt, convertedDec);
                fase = 3;
                
            } else if (fase == 3) {
                fase = 0;
                fromCoin = 1;
                toCoin = 2;
                valor[0] = valor[1] = valor[2] = valor[3] = valor[4] = 0;
                cursorPos = 0;
                lcdClear();
                showCurrency(fromCoin, "De: ");
            }

            __delay_cycles(300000);
        }
        lastButtonState = buttonState;
    }
}
