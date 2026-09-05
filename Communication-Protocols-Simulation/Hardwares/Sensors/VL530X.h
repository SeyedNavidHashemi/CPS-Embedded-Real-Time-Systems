#ifndef VL53_X_H
#define VL53_X_H

#include <CPS4042/Hardwares/Board.h>
#include <CPS4042/Units/BaudRate.h>
#include <CPS4042/Wires/Pin.h>
#include <boost/pfr.hpp>
#include <iostream>
#include <random>

namespace Sensors
{
using Vl530xVoltage = VoltageLevel3_3v;

template <std::uint64_t bar, std::uint64_t btr, typename WorkingVoltageTp>
requires std::is_base_of_v<AbstractVoltageLevel, WorkingVoltageTp>
struct Vl530xGpio
{
public:
    Pins::Vdd<WorkingVoltageTp> vdd {bar, btr, "Vl530x::vdd"};
    Pins::Gnd<WorkingVoltageTp> gnd {bar, btr, "Vl530x::gnd"};
    Pins::Sda<WorkingVoltageTp> sda {bar, btr, "Vl530x::sda"};
    Pins::Scl<WorkingVoltageTp> scl {bar, btr, "Vl530x::scl"};
};

class Vl530x : public Board<BaudRates::NotSpecified,
                            BitRates::same(BaudRates::NotSpecified),
                            Frequency::Drived, Vl530xVoltage, Vl530xGpio>
{
public:
    inline static constexpr Byte address = 0x29;

    explicit Vl530x() :
        Parent {"Vl530x::processor"}
    {
        m_processor->installProtocol(&i2c);
        std::cout << "one instance of Vl530x" << " created." << std::endl;
    }

    class I2C : public Protocols::AbstractI2C<Vl530x, Gpio>
    {
    public:
        explicit I2C(Vl530x* b) :
            Protocols::AbstractI2C<Vl530x, Gpio> {b}
        {}

        bool addressMatched = false;
        bool ackSent = false;

        Byte tx[3];
        int txIndex = 0;
        bool dataSent = false;
        int minValue {0};
        int maxValue {4000};

        void set_range(int minV, int maxV)
        {
            minValue = minV;
            maxValue = maxV;
        }

        void recieve_and_check_address()
        {
            if(m_board->gpio().sda.hasByteToRead())
            {
                Byte rec_byte = read();

                if(rec_byte == Vl530x::address)
                {
                    addressMatched = true;
                    std::cout << "[Sensor] Address matched!" << std::endl;

                    send_ACK_to_Esp();
                    generate_2_bytes_and_checksum();
                }
            }
        }

        void send_ACK_to_Esp()
        {
            Bit ack = Bit::One;
            m_board->gpio().sda.write(ack);
            std::cout << "[Sensor] ACK sent = " << (int)ack << std::endl;
            ackSent = true;
        }

        void send_3_bytes_of_data()
        {
            Byte curByte = tx[txIndex];
            write(curByte);

            std::cout << "[Sensor] Sending Byte " << txIndex
                    << ": " << (int)(UByte)curByte << std::endl;

            txIndex++;

            if(txIndex == 3)
            {
                std::cout << "[Sensor] Transmission finished\n";
                dataSent = true;
            }
        }

        void reset_transaction()
        {
            addressMatched = false;
            ackSent = false;
            dataSent = false;
            txIndex = 0;

            // release I2C data line for next transaction
            m_board->gpio().sda.write(Bit::Z);

            std::cout << "[Sensor] Ready for next transaction" << std::endl;
        }

        std::uint16_t make_random_distance()
        {
            static std::mt19937 rng{std::random_device{}()};

            std::uniform_int_distribution<std::uint16_t> dist(
                static_cast<std::uint16_t>(minValue),
                static_cast<std::uint16_t>(maxValue)
            );

            return dist(rng);
        }

        void generate_2_bytes_and_checksum()
        {
            std::uint16_t value = make_random_distance();
            std::cout << "[Sensor] generated value = "
                << value
                << " range = "
                << minValue << " to " << maxValue
                << std::endl;

            ByteVector<std::uint16_t> vec(value);
            tx[0] = getByte<0>(vec);
            tx[1] = getByte<1>(vec);

            tx[2] = std::abs((int)(UByte)tx[0] - (int)(UByte)tx[1]);

            txIndex = 0;
            std::cout << "[Sensor] Preparing for sending data to Esp." << std::endl;
        }

        void init(Byte address) override
        {
            srand(time(nullptr));
            addressMatched = false;
            ackSent = false;
            dataSent = false;
            txIndex = 0;
        }

        void write(Byte byte) override
        {
            m_board->gpio().sda.write(byte);
        }

        Byte read() override
        {
            Byte r = m_board->gpio().sda.read();
            return r;
        }

        void run(Gpio& gpio) override
        {}
    } mutable i2c {this};

protected:
    inline void startModule() override
    {
        m_gpio.scl.onNextEdge([this](Vl530xVoltage level) {
            auto bit = Voltage::toBit(level);

            if(bit == Bit::One)
            {
                m_processor->nextCycle(m_gpio);
            }
        });
    }
};

}    // namespace Sensors

#endif    // VL53_X_H
