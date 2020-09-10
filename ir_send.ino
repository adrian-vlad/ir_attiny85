#include <avr/sleep.h>

#define HDR_MARK    3350
#define HDR_SPACE   1750
#define BIT_MARK    400
#define ONE_SPACE   1200
#define ZERO_SPACE  400
#define HANG_OUT    5

#define PIN_IR  3

void setupIR()
{
    pinMode(PIN_IR, OUTPUT);
}

void mark(long microsecs)
{
    cli();

    while (microsecs > 0) 
    {
        // 38 kHz is about 13 microseconds high and 13 microseconds low
        // adjust HANG_OUT in order for the whole loop to take 26 microseconds
        // it's possible that digitalWrite to take more or less depending on processor
        digitalWrite(PIN_IR, HIGH);
        delayMicroseconds(HANG_OUT);
        digitalWrite(PIN_IR, LOW);
        delayMicroseconds(HANG_OUT);

        microsecs -= 26;
    }

    sei();  // this turns them back on
}

void space(long microsecs)
{
    delayMicroseconds(microsecs);
}

void sendHeader()
{
    mark(HDR_MARK);
    space(HDR_SPACE);
}

void sendFooter()
{
    mark(BIT_MARK);
    space(0);  // Always end with the LED off
}

void send(unsigned long data, int nbits)
{
    for (unsigned long mask = 1UL << (nbits - 1); mask; mask >>= 1)
    {
        if (data & mask) 
        {
            mark(BIT_MARK);
            space(ONE_SPACE);
        }
        else
        {
            mark(BIT_MARK);
            space(ZERO_SPACE);
        }
    }
}

#define PIN_RESET   5
#define PIN_BUTTON_STATUS  2
#define PIN_BUTTON_STATUS_INTERRUPT  PCINT2
#define PIN_LED     1

bool should_send = false;
int status = 0;

ISR(PCINT0_vect)
{
    status = digitalRead(PIN_BUTTON_STATUS);
    should_send = true;
}

void setup() 
{
    setupIR();

    pinMode(PIN_RESET, INPUT_PULLUP);

    cli();
    PCMSK |= (1 << PIN_BUTTON_STATUS_INTERRUPT);
    GIMSK |= (1 << PCIE);
    pinMode(PIN_BUTTON_STATUS, INPUT_PULLUP);
    sei();

    _SFR_BYTE(ADCSRA) &= ~_BV(ADEN);  //cbi(ADCSRA, ADEN)

    pinMode(PIN_LED, OUTPUT);

    digitalWrite(PIN_LED, HIGH);
    delay(1000);
    digitalWrite(PIN_LED, LOW);
}

const byte codes_on[] = {
    0b11000100,
    0b11010011,
    0b01100100,
    0b10000000,
    0b00000000,
    0b00100100,
    0b11000000,
    0b11100000,
    0b01010100,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b10110110
};
const byte codes_off[] = {
    0b11000100,
    0b11010011,
    0b01100100,
    0b10000000,
    0b00000000,
    0b00000100,
    0b11000000,
    0b11110000,
    0b01010100,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b10001110
};

void process()
{
    if (!should_send)
        return;

    if (status == HIGH)
    {
        digitalWrite(PIN_LED, HIGH);

        sendHeader();

        for (int i = 0; i < sizeof(codes_on) / sizeof(codes_on[0]); i++)
        {
            send(codes_on[i], 8);
        }

        sendFooter();

        digitalWrite(PIN_LED, LOW);
    }
    else
    {
        digitalWrite(PIN_LED, HIGH);

        sendHeader();

        for (int i = 0; i < sizeof(codes_off) / sizeof(codes_off[0]); i++)
        {
            send(codes_off[i], 8);
        }

        sendFooter();

        digitalWrite(PIN_LED, LOW);
    }

    should_send = false;
}

void loop()
{
    process();

    system_sleep();
}

void system_sleep()
{
    set_sleep_mode(SLEEP_MODE_PWR_DOWN); // sleep mode is set here
    sleep_enable();
    sleep_mode();                        // System actually sleeps here
    sleep_disable();                     // System continues execution here when watchdog timed out 
}
