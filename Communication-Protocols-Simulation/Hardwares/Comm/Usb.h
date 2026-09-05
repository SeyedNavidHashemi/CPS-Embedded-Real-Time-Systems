#ifndef USB_H
#define USB_H

#include <CPS4042/Hardwares/Board.h>
#include <CPS4042/Protocols/Protocol.h>
#include <CPS4042/Units/BaudRate.h>
#include <CPS4042/Wires/Pin.h>
#include <boost/pfr.hpp>
#include <iostream>

namespace Comm {
using usbVoltage = VoltageLevel3_3v;
template <std::uint64_t bar, std::uint64_t btr, typename WorkingVoltageTp>
requires std::is_base_of_v<AbstractVoltageLevel, WorkingVoltageTp>
    struct usbGpio
    {
    public:
    Pins::Vdd<WorkingVoltageTp> vdd {bar, btr, "Usb::vdd"};    // Pin 0
    Pins::Gnd<WorkingVoltageTp> gnd {bar, btr, "Usb::gnd"};    // Pin 1
    Pins::Tx<WorkingVoltageTp> TX {bar, btr, "Usb::TX"};    // Pin 2
    Pins::Rx<WorkingVoltageTp> RX {bar, btr, "Usb::RX"};    // Pin 3
    };

    class Usb : public Board<BaudRates::NotSpecified,
                            BitRates::same(BaudRates::NotSpecified),
                            Frequency::F320khz, usbVoltage, usbGpio>
    {
    private:

    public:
    inline static constexpr Byte address = 0x30;

        explicit Usb() :
        Parent {"Usb::processor"}
    {
        m_processor->installProtocol(&usart);
        std::cout << "one instance of Usb" << " created." << std::endl;
    }
    
    class USART : public Protocols::AbstractUsart<Usb, Gpio>
    {
    public:
        explicit USART(Usb* b) :
            Protocols::AbstractUsart<Usb, Gpio> {b},
            m_usb {b}
        {
            sensed_zero = false;
            sensed_one = false;
            received_byte = false;
            done = false;
            frame = 0;
        }
        void
        reset_vars()
        { 
            sensed_zero = false;
            sensed_one = false;
            received_byte = false;
            done = true;
        }
        
        bool
        is_done()
        {
            return done;
        }

        void
        write(Byte byte) override
        {
            m_usb->gpio().RX.write(Bit::Zero);
            m_usb->gpio().RX.write(byte);
            m_usb->gpio().RX.write(Bit::One);
            return;
        }
        Byte
        read() override
        {
            if(m_usb->gpio().TX.hasByteToRead()){
                frame = m_usb->gpio().TX.read();
                received_byte = true;
                return frame;
            }
            return 0;
        }
        void set_sensed_zero() {
            if(m_usb->gpio().TX.hasBitToRead()){
                if(m_usb->gpio().TX.readBit() == Bit::Zero) {
                    sensed_zero = true;
                }
            }
        }

        void set_sensed_one() {
            if(m_usb->gpio().TX.hasBitToRead()) {
                if(m_usb->gpio().TX.readBit() == Bit::One) {
                    sensed_one = true;
                }
            }
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


    private:
        Usb* m_usb;
        bool sensed_zero;
        bool received_byte;
        bool sensed_one;
        bool done;
        Byte frame;

    } mutable usart {this};

    protected:
    inline void
    startModule() override
    {
        std::cout << "started USB" << std::endl;
    }
};
}

#endif