#ifndef ESP8266_H
#define ESP8266_H

#include <CPS4042/Hardwares/Board.h>
#include <CPS4042/Protocols/Protocol.h>
#include <CPS4042/Units/BaudRate.h>
#include <CPS4042/Wires/Pin.h>
#include <boost/pfr.hpp>

namespace Boards
{

using Esp8266Voltage = VoltageLevel3_3v;

template <BaudRate BR, BitRate BTR, typename WorkingVoltageTp>
requires std::is_base_of_v<AbstractVoltageLevel, WorkingVoltageTp>
struct Esp8266Gpio
{
public:
    Pins::Vdd<WorkingVoltageTp>     vdd1 {BR, BTR, "Esp8266::vdd1"};    // Pin 0
    Pins::Gnd<WorkingVoltageTp>     gnd1 {BR, BTR, "Esp8266::gnd1"};    // Pin 1

    Pins::Vdd<WorkingVoltageTp>     vdd2 {BR, BTR, "Esp8266::vdd2"};    // Pin 2
    Pins::Gnd<WorkingVoltageTp>     gnd2 {BR, BTR, "Esp8266::gnd2"};    // Pin 3

    Pins::Vdd<WorkingVoltageTp>     vdd3 {BR, BTR, "Esp8266::vdd3"};    // Pin 4
    Pins::Gnd<WorkingVoltageTp>     gnd3 {BR, BTR, "Esp8266::gnd3"};    // Pin 5

    Pins::Rx<WorkingVoltageTp>      rx {BR, BTR, "Esp8266::rx"};        // Pin 6
    Pins::Tx<WorkingVoltageTp>      tx {BR, BTR, "Esp8266::tx"};        // Pin 7

    Pins::Sda<WorkingVoltageTp>     sda {BR, BTR, "Esp8266::sda"};      // Pin 8
    Pins::Scl<WorkingVoltageTp>     scl {BR, BTR, "Esp8266::scl"};      // Pin 9

    Pins::Digital<WorkingVoltageTp> d0 {BR, BTR, "Esp8266::d0"};    // Pin 10
    Pins::Digital<WorkingVoltageTp> d1 {BR, BTR, "Esp8266::d1"};    // Pin 11
    Pins::Digital<WorkingVoltageTp> d2 {BR, BTR, "Esp8266::d2"};    // Pin 12
    Pins::Digital<WorkingVoltageTp> d3 {BR, BTR, "Esp8266::d3"};    // Pin 13
    Pins::Digital<WorkingVoltageTp> d4 {BR, BTR, "Esp8266::d4"};    // Pin 14
    Pins::Digital<WorkingVoltageTp> d5 {BR, BTR, "Esp8266::d5"};    // Pin 15
    Pins::Analog<WorkingVoltageTp>  a0 {BR, BTR, "Esp8266::a1"};    // Pin 16
};

class Esp8266
    : public Board<BaudRates::NotSpecified, BitRates::same(BaudRates::NotSpecified),
                   Frequency::F320khz, Esp8266Voltage, Esp8266Gpio>
{
public:
    explicit Esp8266() :
        Parent {"Esp8266::Processor"}
    {
        m_processor->communicationClockChanged.connect(
          [this](Bit edge) { m_gpio.scl.nextEdge(edge); });

        m_processor->installProtocol(&i2c);
        m_processor->installProtocol(&usart);

        std::cout << "one instance of Esp8266" << " created." << std::endl;
    };

    class I2C : public Protocols::AbstractI2C<Esp8266, Gpio>
    {
    public:
        explicit I2C(Esp8266* b) :
            Protocols::AbstractI2C<Esp8266, Gpio> {b}
        {}

        Byte pendingByte{};
        Byte currentByte{};

        bool start_communication = false; // this is for starting a comminucation
        bool address_sent = false;

        bool waitingForAck = false;
        bool ackReceived = false;

        Byte receivedBytes[3];     
        int byteCount = 0;

        void start_communicating(bool start)
        {
            start_communication = true;
            return;
        }

        void send_address()
        {
            write(pendingByte);
            std::cout << "[ESP] Sending address: "
                      << (int)pendingByte << std::endl;
            m_board->gpio().sda.write(Bit::Z); // release line
            address_sent = true;
            waitingForAck = true;

            return;
        }

        void wait_for_recieving_ACK()
        {
            if(m_board->gpio().sda.hasBitToRead())
            {
                Bit r = m_board->gpio().sda.readBit();

                std::cout << "[ESP] ACK received = " << (int)r << std::endl;

                ackReceived = (r == Bit::One);
                waitingForAck = false;

                if(!ackReceived)
                {
                    std::cout << "[ESP] No ACK!" << std::endl;
                    return;
                }
                // preparing to recieve data from the sensor
                byteCount = 0;
                currentByte = 0;
            }
        }

        void recieve_3_bytes_of_data()
        {
            if(m_board->gpio().sda.hasByteToRead())
            {
                Byte currentByte = read();

                receivedBytes[byteCount] = currentByte;
                std::cout << "[ESP] Received byte "
                        << byteCount << " = "
                        << (int)(UByte)currentByte << std::endl;

                byteCount++;
                currentByte = 0;

                if(byteCount == 3)
                {
                    validate_3_bytes();
                }
            }

            return;
        }

        void validate_3_bytes()
        {
            uint8_t b0 = (UByte)receivedBytes[0];
            uint8_t b1 = (UByte)receivedBytes[1];
            uint8_t chk = (UByte)receivedBytes[2];

            uint8_t expected =
                std::abs((int)b0 - (int)b1);

            bool ok = chk == expected;
            if(ok)
            {
                uint16_t value = ((uint16_t)b1 << 8) | b0;

                std::cout << "[ESP] VALUE = " << int(value)
                        << "  (checksum OK)" << std::endl;
            }
            else
            {
                std::cout << "[ESP] Checksum FAILED!  sensor="
                        << (int)chk << "  should be="
                        << std::abs((int)b0 - (int)b1)
                        << std::endl;
            }
        }

        void init(Byte address) override
        {
            start_communication = false;
            address_sent = false;

            pendingByte = address;

            waitingForAck = false;
            ackReceived = false;

            byteCount = 0;

            std::cout << "[ESP] Init I2C" << std::endl;
        }

        void write(Byte byte) override 
        {
            m_board->gpio().sda.write(byte);       
            return;
        }

        Byte read() override
        {
            Byte r = m_board->gpio().sda.read();
            return r;
        }


        void run(Gpio& gpio) override
        {

        }

    } mutable i2c {this};

    class USART : public Protocols::AbstractUsart<Esp8266, Gpio>
    {
    public:
        explicit USART(Esp8266* b) :
            Protocols::AbstractUsart<Esp8266, Gpio> {b},
            Esp {b}
        {
            sensed_zero = false;
            sensed_one = false;
            received_byte = false;
            got_address = false;
            frame = 0;
        }

        void
        start()
        {
            got_address = true;
        }

        bool
        is_ready_to_send()
        {
            return got_address;
        }

        void
        run(Gpio& gpio) override
        {
        }

        bool has_sensed_zero() {
            return sensed_zero;
        }

        bool has_sensed_one() {
            return sensed_one;
        }

        bool has_received_byte() {
            return received_byte;
        }

        bool has_byte_to_read()
        {
            return this->m_board->gpio().rx.hasByteToRead();
        }

        void
        write(Byte byte) override
        {
            Esp->gpio().tx.write(Bit::Zero);
            Esp->gpio().tx.write(byte);
            Esp->gpio().tx.write(Bit::One);
            return;
        }
        Byte
        read() override
        {
            if(Esp->gpio().rx.hasByteToRead()){
                frame = Esp->gpio().rx.read();
                received_byte = true;
                return frame;
            }
            return 0;
        }
        void set_sensed_zero() {
            if(Esp->gpio().rx.hasBitToRead()) {
                if(Esp->gpio().rx.readBit() == Bit::Zero) {
                    sensed_zero = true;
                }
            }
        }
        void set_sensed_one() {
            if(Esp->gpio().rx.hasBitToRead()) {
            if(Esp->gpio().rx.readBit() == Bit::One) {
                sensed_one = true;
                }
            }
        }

        void reset_receiver_only() {
            sensed_zero = false;
            received_byte = false;
            sensed_one = false;
        }
        
        void reset() {
            sensed_zero = false;
            received_byte = false;
            sensed_one = false;
            got_address = false;
        }

        private:
            Esp8266* Esp;
            bool sensed_zero;
            bool received_byte;
            bool sensed_one;
            Byte frame;
            Byte address;
            bool got_address;
    } mutable usart {this};

protected:
    inline void
    startModule() override
    {}
};
}    // namespace Boards

#endif    // ESP8266_H
