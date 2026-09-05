#ifndef I2C_MUX_H
#define I2C_MUX_H

#include <CPS4042/Hardwares/Board.h>
#include <CPS4042/Protocols/Protocol.h>
#include <CPS4042/Units/BaudRate.h>
#include <CPS4042/Units/Frequency.h>
#include <CPS4042/Units/Voltage.h>
#include <CPS4042/Units/Bit.h>
#include <CPS4042/Units/Byte.h>
#include <CPS4042/Wires/Pin.h>

#include <iostream>
#include <type_traits>

namespace Comm
{

using I2CMuxVoltage = VoltageLevel3_3v;

template <BaudRate BR, BitRate BTR, typename WorkingVoltageTp>
requires std::is_base_of_v<AbstractVoltageLevel, WorkingVoltageTp>
struct I2CMuxGpio
{
public:
    Pins::Vdd<WorkingVoltageTp> vdd {BR, BTR, "I2CMux::vdd"};
    Pins::Gnd<WorkingVoltageTp> gnd {BR, BTR, "I2CMux::gnd"};

    Pins::Tx<WorkingVoltageTp> TX {BR, BTR, "I2CMux::TX"};
    Pins::Rx<WorkingVoltageTp> RX {BR, BTR, "I2CMux::RX"};

    Pins::Sda<WorkingVoltageTp> sda0 {BR, BTR, "I2CMux::sda0"};
    Pins::Scl<WorkingVoltageTp> scl0 {BR, BTR, "I2CMux::scl0"};

    Pins::Sda<WorkingVoltageTp> sda1 {BR, BTR, "I2CMux::sda1"};
    Pins::Scl<WorkingVoltageTp> scl1 {BR, BTR, "I2CMux::scl1"};
};

class I2CMux : public Board<BaudRates::NotSpecified,
                            BitRates::same(BaudRates::NotSpecified),
                            Frequency::F320khz,
                            I2CMuxVoltage,
                            I2CMuxGpio>
{
public:
    using Parent = Board<BaudRates::NotSpecified,
                         BitRates::same(BaudRates::NotSpecified),
                         Frequency::F320khz,
                         I2CMuxVoltage,
                         I2CMuxGpio>;

    using Gpio = typename Parent::Gpio;

    class I2C : public Protocols::AbstractI2C<I2CMux, Gpio>
    {
    public:
        explicit I2C(I2CMux* board, int channel)
            : Protocols::AbstractI2C<I2CMux, Gpio> {board},
              channel {channel}
        {}

        auto& sda()
        {
            if(channel == 0)
            {
                return this->m_board->m_gpio.sda0;
            }

            return this->m_board->m_gpio.sda1;
        }

        void init(Byte sensor_address) override
        {
            pending_sensor_address = sensor_address;

            address_sent = false;
            waiting_for_ack = false;
            ack_received = false;

            received_byte_count = 0;
        }

        void send_address()
        {
            write(pending_sensor_address);

            // Release the SDA line so the sensor can send ACK.
            sda().write(Bit::Z);

            address_sent = true;
            waiting_for_ack = true;
        }

        void wait_for_receiving_ack()
        {
            if(!sda().hasBitToRead())
            {
                return;
            }

            Bit ack_bit = sda().readBit();

            ack_received = (ack_bit == Bit::One);
            waiting_for_ack = false;

            if(ack_received)
            {
                received_byte_count = 0;
            }
        }

        bool receive_sensor_packet()
        {
            if(!sda().hasByteToRead())
            {
                return false;
            }

            Byte received_byte = read();
            received_sensor_packet[received_byte_count] = received_byte;

            received_byte_count++;

            return received_byte_count == sensor_packet_size;
        }

        Byte get_received_byte(int index) const
        {
            return received_sensor_packet[index];
        }

        void reset_transaction()
        {
            address_sent = false;
            waiting_for_ack = false;
            ack_received = false;
            received_byte_count = 0;
        }

        void write(Byte byte) override
        {
            sda().write(byte);
        }

        Byte read() override
        {
            return sda().read();
        }

        void run(Gpio&) override
        {
        }

        bool has_address_sent() const
        {
            return address_sent;
        }

        bool is_waiting_for_ack() const
        {
            return waiting_for_ack;
        }

        bool has_ack_received() const
        {
            return ack_received;
        }

    private:
        static constexpr int sensor_packet_size = 3;

        int channel {0};

        Byte pending_sensor_address {0};
        Byte received_sensor_packet[sensor_packet_size] {};

        bool address_sent {false};
        bool waiting_for_ack {false};
        bool ack_received {false};

        int received_byte_count {0};
    };

    mutable I2C i2c0 {this, 0};
    mutable I2C i2c1 {this, 1};

    class USART : public Protocols::AbstractUsart<I2CMux, Gpio>
    {
    public:
        explicit USART(I2CMux* board)
            : Protocols::AbstractUsart<I2CMux, Gpio> {board}
        {}

        void start()
        {
            enabled = true;
        }

        void write(Byte byte) override
        {
            this->m_board->gpio().RX.write(Bit::Zero);
            this->m_board->gpio().RX.write(byte);
            this->m_board->gpio().RX.write(Bit::One);
        }

        Byte read() override
        {
            if(this->m_board->gpio().TX.hasByteToRead())
            {
                received_frame = this->m_board->gpio().TX.read();
                received_byte = true;
                return received_frame;
            }

            return 0;
        }

        void set_sensed_zero()
        {
            if(this->m_board->gpio().TX.hasBitToRead() &&
               this->m_board->gpio().TX.readBit() == Bit::Zero)
            {
                sensed_zero = true;
            }
        }

        void set_sensed_one()
        {
            if(this->m_board->gpio().TX.hasBitToRead() &&
               this->m_board->gpio().TX.readBit() == Bit::One)
            {
                sensed_one = true;
            }
        }

        bool has_sensed_zero() const
        {
            return sensed_zero;
        }

        bool has_sensed_one() const
        {
            return sensed_one;
        }

        bool has_received_byte() const
        {
            return received_byte;
        }

        void reset_receiver_only()
        {
            sensed_zero = false;
            sensed_one = false;
            received_byte = false;
            received_frame = 0;
        }

        void reset()
        {
            reset_receiver_only();
            enabled = false;
        }

        void run(Gpio&) override
        {
        }

    private:
        bool enabled {false};

        bool sensed_zero {false};
        bool sensed_one {false};
        bool received_byte {false};

        Byte received_frame {0};
    };

    mutable USART usart {this};

    explicit I2CMux()
        : Parent {"I2CMux::Processor"}
    {
        m_processor->communicationClockChanged.connect(
            [this](Bit edge) {
                m_gpio.scl0.nextEdge(edge);
                m_gpio.scl1.nextEdge(edge);
            }
        );

        m_gpio.sda0.write(Bit::Z);
        m_gpio.sda1.write(Bit::Z);

        m_processor->installProtocol(&usart);
        m_processor->installProtocol(&i2c0);
        m_processor->installProtocol(&i2c1);

        std::cout << "one instance of I2CMux created." << std::endl;
    }

protected:
    inline void startModule() override
    {
    }
};

} // namespace Comm

#endif // I2C_MUX_H