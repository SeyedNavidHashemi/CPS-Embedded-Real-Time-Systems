#ifndef I2C_MULTIPLEXER_H
#define I2C_MULTIPLEXER_H

#include <CPS4042/Hardwares/Comm/I2CMux.h>
#include <CPS4042/Hardwares/Sensors/VL530X.h>
#include <CPS4042/Sketchs/AbstractSketch.h>

#include <iostream>

class I2CMultiplexer : public AbstractSketch<Comm::I2CMux>
{
public:
    explicit I2CMultiplexer(Comm::I2CMux* node)
        : AbstractSketch<Comm::I2CMux> {node}
    {}

    std::int32_t setup(Comm::I2CMux::Gpio&) override
    {
        std::cout << "[I2C MUX] setup completed." << std::endl;

        node()->usart.start();

        return 0;
    }

    std::int32_t loop(Comm::I2CMux::Gpio&) override
    {
        receive_channel_command();
        handle_sensor_ack();
        handle_sensor_data();

        return 0;
    }

private:
    Comm::I2CMux::I2C& active_i2c()
    {
        if(selected_channel == 0)
        {
            return node()->i2c0;
        }

        return node()->i2c1;
    }

    void receive_channel_command()
    {
        if(!node()->usart.has_sensed_zero())
        {
            node()->usart.set_sensed_zero();
        }

        if(node()->usart.has_sensed_zero() &&
           !node()->usart.has_received_byte())
        {
            channel_command = node()->usart.read();
        }

        if(node()->usart.has_sensed_zero() &&
           node()->usart.has_received_byte() &&
           !node()->usart.has_sensed_one())
        {
            node()->usart.set_sensed_one();
        }

        if(node()->usart.has_sensed_zero() &&
           node()->usart.has_received_byte() &&
           node()->usart.has_sensed_one())
        {
            selected_channel =
                static_cast<int>(static_cast<UByte>(channel_command)) % 2;

            std::cout << "[I2C MUX] selected channel "
                      << selected_channel
                      << std::endl;

            active_i2c().init(Sensors::Vl530x::address);
            active_i2c().send_address();

            std::cout << "[I2C MUX] sent sensor address on channel "
                      << selected_channel
                      << std::endl;

            waiting_for_sensor_ack = true;

            node()->usart.reset_receiver_only();
        }
    }

    void handle_sensor_ack()
    {
        if(!waiting_for_sensor_ack)
        {
            return;
        }

        active_i2c().wait_for_receiving_ack();

        if(active_i2c().is_waiting_for_ack())
        {
            return;
        }

        if(active_i2c().has_ack_received())
        {
            std::cout << "[I2C MUX] sensor ACK received on channel "
                      << selected_channel
                      << std::endl;

            waiting_for_sensor_data = true;
        }
        else
        {
            std::cout << "[I2C MUX] sensor ACK failed on channel "
                      << selected_channel
                      << std::endl;
        }

        waiting_for_sensor_ack = false;
    }

    void handle_sensor_data()
    {
        if(!waiting_for_sensor_data)
        {
            return;
        }

        if(!active_i2c().receive_sensor_packet())
        {
            return;
        }

        Byte sensor_value_low_byte = active_i2c().get_received_byte(0);
        Byte sensor_value_high_byte = active_i2c().get_received_byte(1);
        Byte sensor_checksum = active_i2c().get_received_byte(2);

        node()->usart.write(sensor_value_low_byte);
        node()->usart.write(sensor_value_high_byte);
        node()->usart.write(sensor_checksum);

        std::cout << "[I2C MUX] forwarded sensor packet from channel "
                  << selected_channel
                  << std::endl;

        waiting_for_sensor_data = false;
        active_i2c().reset_transaction();
    }

private:
    Byte channel_command {0};
    int selected_channel {0};
    bool waiting_for_sensor_ack {false};
    bool waiting_for_sensor_data {false};
};

#endif // I2C_MULTIPLEXER_H