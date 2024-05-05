#ifndef JBDBMS
#define JBDBMS

/* 
Class to communicate with a Jabaida Battery Management System via RS485

The device understands 3 read commands and one write command: 
1) Read capacity, balance, temperature and status of mosfets
2) Read cell voltages (2 bytes for millivolts of each cell)
3) Read ascii hardware id (type or serial?)
4) Write status of charge and discharge mosfets on or off

Commands sent to the device look like this:
1st byte of each frame is always 0xdd
2nd byte is 0xa5 for reading and 0x5a for writing 
3rd byte is the command (3=status, 4=voltages, 5=hardware, e1=set mosfets)
4th byte is length of following data (0 if no data)
5th byte starts data of given length, if any
5+length byte is high byte of checksum (calculation: -sum(3rd byte, ..., last data byte))
6+length byte is low byte of checksum
7+length byte is always 0x77

The device answers with 0xdd, command, returncode, length, data, checksum, 0x77
returncode is 0 for ok or 0x80 on error, checksum is also calculated from 3rd byte on.

The checksum of the frames is handled internally by the lib, as well as lengths in the data region. 

Notes: 
* Not every detail is tested, only what I needed for my setup. Bug reports welcome.
* Different wirings are possible. eSmart3 has 5V logic levels and ESP32 uses 3.3V. 
  Some RS485 boards have builtin 5V<->3.3V level shifters and automatic direction selection (recommended).
  Other RS485 boards (like mine) have only 5V DI/RO and use DE/!RE for manual direction selection. 
  For those boards a level shifter for 3 lines is needed (I used a TXS108E/YF08E)
* The RS485 board (and level shifter) needs a 5V supply. The jbd bms 5V from the UART port can be used.
  But these 5V are too weak (TODO: testing?) for powering ESP32 boards (at least with WLAN enabled).

Example wiring with level shifter
            ____________
_____      |         !RE|--+   _____________       __________
j    |     |          DE|--+--|B3         A3|-----|IO22      |
b  A-|-----|A-        DI|-----|B2         A2|-----|RX2       |
d  B+|-----|B+ MAX485 RO|-----|B1 TXB0108 A1|-----|TX2 ESP32 |    _________     ____
b    |     |         VCC|-----|VCCB     VCCA|--+--|3.3V   TX1|---|RX  USB- |   | PC |
m    |     |_________GND|--+--|GND________OE|--+  |       RX1|---|TX Serial|===|USB |
s____|                     +--------------------+-|GND____Vin|---|VCC      |   |____|
                                                +----------------|GND______|

Author: Joachim.Banzhaf@gmail.com
License: GPL V2
*/

#include <Arduino.h>
#include <Stream.h>

// Don't use padding in structures to match what jbd bms devices need
#pragma pack(2)

class JbdBms {
public:
    // Datatypes used by the device

    typedef enum direction { READ=0xa5, WRITE=0x5a } direction_t;

    typedef struct request_header {
        uint8_t start, direction, command, length;
    } request_header_t;

    typedef struct response_header {
        uint8_t start, command, returncode, length;
    } response_header_t;

    // Enum values represent states of 2 bits
    typedef enum mosfet { MOSFET_NONE, MOSFET_CHARGE, MOSFET_DISCHARGE, MOSFET_BOTH } mosfet_t;

    typedef enum cmd {
        STATUS = 3,
        CELLS,
        HARDWARE,
        MOSFET = 0xe1
    } cmd_t;

    typedef enum returncode {
        OK,
        ERR = 0x80
    } returncode_t;

    typedef struct temperature {
        uint8_t hi, lo;  // big endian uint16_t starting at odd address 
    } temperature_t;

    typedef struct Status {
        uint16_t voltage;            // in 10 mV
        int16_t current;             // in 10 mA, positive means charge, negative discharge
        uint16_t remainingCapacity;  // in 10 mAh last full capacity
        uint16_t nominalCapacity;    // in 10 mAh 
        uint16_t cycles;
        uint16_t productionDate;     // |7 bits year since 2000|4 bit month 1..12|5bit day 1..31|
        uint16_t balanceLow;         // bit is set if cell is balanced (cell 1..16)
        uint16_t balanceHigh;        // bit is set if cell is balanced (cell 17..32)
        uint16_t fault;              // bit is set if fault protection is active (see static protection functions)
        uint8_t version;             // bms firmware version
        uint8_t currentCapacity;     // percentage 
        uint8_t mosfetStatus;        // see mosfet_t
        uint8_t cells;
        uint8_t ntcs;                // following this are the ntc temperatures in 0.1K, 2 bytes each
        temperature_t temperatures[8];  // Jiabaida software uses 8 fields. Enough for my BMS type
    } Status_t;

    typedef struct Cells {
        uint16_t voltages[32];  // max 32 cells supported
    } Cells_t;

    typedef struct Hardware {
        char id[32];  // max 31 chars + EOS (not sent)
    } Hardware_t;

    // Basic methods
    // Object represents device at serial port. Send commands with minimal delay given.
    // If prev is not NULL, JbdBms uses it to store millis() of last stream access and
    // expects other stream users to do the same (Joba_ESmart3 does the same)
    JbdBms( Stream &serial, uint32_t* prev = NULL, uint8_t command_delay_ms = 60)
    : _serial(serial), _delay(command_delay_ms), _prev(prev), _dir_pin(-1) {
        if (!_prev) {
            _prev = &_prev_local;
        }
    }

    // Init dir_pin. -1 if RS485 hardware sets direction automatically
    void begin( int dir_pin = -1 ) {
        _dir_pin = dir_pin;
        if( _dir_pin >= 0 ) {
            pinMode(_dir_pin, OUTPUT);
            digitalWrite(_dir_pin, LOW);  // read mode (default)
        }
    }

    // Send header and command then receive header and result (not including crc)
    // Return true if header and command are written and result and header are read successfully
    bool execute( request_header_t &header, uint8_t *command, uint8_t *result ) {
        uint16_t crc;
        uint8_t stop = 0x77;

        if( !prepareCmd(header, command, crc) ) {
            return false;
        }

        uint32_t remaining = _delay - (millis() - *_prev);
        if( remaining <= _delay ) {
            delay(remaining);
        }

        if( _dir_pin >= 0 ) {
            digitalWrite(_dir_pin, HIGH);  // write mode
        }

        _serial.flush();  // make sure read buffer is empty
        bool rc = (_serial.write((uint8_t *)&header, sizeof(header)) == sizeof(header))
            && (_serial.write(command, header.length) == header.length)
            && (_serial.write((uint8_t *)&crc, sizeof(crc)) == sizeof(crc))
            && (_serial.write(&stop, sizeof(stop)) == sizeof(stop));
        _serial.flush();  // wait until write is done 

        if( _dir_pin >= 0 ) {
            digitalWrite(_dir_pin, LOW);  // read mode (default)
        }

        if( rc ) {
            response_header_t header = {0};
            rc = (_serial.readBytes((uint8_t *)&header, sizeof(header)) == sizeof(header))
            && (header.start == 0xdd && header.length <= 64)
            && (header.length == 0 || (result && _serial.readBytes(result, header.length) == header.length))
            && (_serial.readBytes((uint8_t *)&crc, sizeof(crc)) == sizeof(crc))
            && (_serial.readBytes(&stop, sizeof(stop)) == sizeof(stop))
            && isValid(header, result, crc)
            && header.returncode == 0;
        }
        *_prev = millis();
        return rc;
    }

    // Commands. Return true if execute() was successful
    bool getStatus( Status_t &data ) {
        request_header_t header = { 0, READ, STATUS, 0 };
        bool rc = execute(header, 0, (uint8_t *)&data);
        swap(&data.voltage);
        swap((uint16_t *)&data.current);
        swap(&data.remainingCapacity);
        swap(&data.nominalCapacity);
        swap(&data.cycles);
        swap(&data.productionDate);
        swap(&data.balanceLow);
        swap(&data.balanceHigh);
        swap(&data.fault);
        return rc;
    }

    bool getCells( Cells_t &data ) {
        request_header_t header = { 0, READ, CELLS, 0 };
        bool rc = execute(header, 0, (uint8_t *)&data);
        for (size_t i = 0; i < sizeof(data.voltages)/sizeof(*data.voltages); i++) {
            swap(&data.voltages[i]);
        }
        return rc;
    }

    bool getHardware( Hardware_t &data ) {
        request_header_t header = { 0, READ, HARDWARE, 0 };
        return execute(header, 0, (uint8_t *)&data);
    }

    bool setMosfetStatus( mosfet_t status ) {
        request_header_t header = { 0, WRITE, MOSFET, 2 };
        uint8_t status_inv = ~status & MOSFET_BOTH;  // invert status pins
        uint8_t mosfetStatus[] = { 0, status_inv };
        return execute(header, mosfetStatus, 0);
    }
    
    const char* balance( const Status_t &data ) {
        static char balanceStr[33];

        char *balancePtr = balanceStr;
        uint32_t balanceBits = (uint32_t)data.balanceHigh << 16 | data.balanceLow;
        size_t cell = (data.cells < sizeof(balanceStr)) ? data.cells : sizeof(balanceStr) - 1;
        while(cell--) {
            *(balancePtr++) = (balanceBits & 1) ? '1' : '0';
            balanceBits >>= 1;
        }
        *balancePtr = '\0';

        return balanceStr;
    }

    // Static helper functions
    static uint16_t swap( uint16_t *data ) { return *data = (*data >> 8) | (*data << 8); }  // lsb first
    static uint16_t deciKelvin( temperature_t temperature ) { return (uint16_t)temperature.hi << 8 | temperature.lo; }
    static int16_t deciCelsius( temperature_t temperature ) { return deciKelvin(temperature) - 2731; }
    static uint16_t year( uint16_t prodDate ) { return (prodDate >> 9) + 2000; }
    static uint8_t month( uint16_t prodDate ) { return (prodDate >> 5) & 0xf; }
    static uint8_t day( uint16_t prodDate ) { return prodDate & 0x1f; }
    static bool isCellOvervoltage( uint16_t fault )           { return fault & 0x0001; }
    static bool isCellUndervoltage( uint16_t fault )          { return fault & 0x0002; }
    static bool isOvervoltage( uint16_t fault )               { return fault & 0x0004; }
    static bool isUndervoltage( uint16_t fault )              { return fault & 0x0008; }
    static bool isChargeOvertemperature( uint16_t fault )     { return fault & 0x0010; }
    static bool isChargeUndertemperature( uint16_t fault )    { return fault & 0x0020; }
    static bool isDischargeOvertemperature( uint16_t fault )  { return fault & 0x0040; }
    static bool isDischargeUndertemperature( uint16_t fault ) { return fault & 0x0080; }
    static bool isChargeOvercurrent( uint16_t fault )         { return fault & 0x0100; }
    static bool isDischargeOvercurrent( uint16_t fault )      { return fault & 0x0200; }
    static bool isShortCircuit( uint16_t fault )              { return fault & 0x0400; }
    static bool isIcError( uint16_t fault )                   { return fault & 0x0800; }
    static bool isMosfetSoftwareLock( uint16_t fault )        { return fault & 0x1000; }

private:
    uint16_t genRequestCrc( request_header_t &header, uint8_t *data ) {
        return genCrc(header.command, header.length, data);
    }

    uint16_t genResponseCrc( response_header_t &header, uint8_t *data ){
        return genCrc(header.returncode, header.length, data);
    }

    uint16_t genCrc( uint8_t byte, uint8_t len, uint8_t *data ) {
        uint16_t crc = 0;

        if( len < 31 && (len == 0 || data)) {
            crc -= byte;
            crc -= len;
            while( len-- ) {
                crc -= *(data++);
            }
        }
        return swap(&crc);
    }
    bool isValid( response_header_t &header, uint8_t *data, uint16_t crc ) {
        return header.start == 0xdd && data[header.length] == 0x77 && genResponseCrc(header, data) == crc;
    }
    bool prepareCmd( request_header_t &header, uint8_t *command, uint16_t &crc ) {
        header.start = 0xdd;
        crc = genRequestCrc(header, command);
        return crc != 0;
    }

    Stream &_serial;
    uint8_t _delay;
    uint32_t _prev_local;
    uint32_t *_prev;
    int _dir_pin;    
};

#endif
