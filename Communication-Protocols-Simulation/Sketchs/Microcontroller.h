#ifndef MICROCONTROLLER_H
#define MICROCONTROLLER_H

#include <CPS4042/Hardwares/Boards/Esp8266.h>
#include <CPS4042/Sketchs/AbstractSketch.h>
#include <CPS4042/Utils/ByteStream.h>
#include <CPS4042/Utils/Wave.h>
#include <bitset>

class MicroController : public AbstractSketch<Boards::Esp8266>
{
public:
    explicit MicroController(Boards::Esp8266* node, int mode = 1) :
        AbstractSketch<Boards::Esp8266> {node},
        mode {mode}

    {
        data = 0;
    }

    std::int32_t
    setup(Boards::Esp8266::Gpio& gpio) override
    {
        std::cout << "esp8266 setup completed." << std::endl;

        // I2C mode
        if(mode == 1){
            node()->i2c.init(0x29);
            node()->i2c.start_communicating(true);
        }
        
        // Usart mode
        if(mode == 2){
            node()->usart.start();
        }
        
        // I2C mux mode
        if(mode == 3){
            node()->usart.start();
        }
        
        return 0;
    }

    std::int32_t
    loop(Boards::Esp8266::Gpio& gpio) override
    {
        // I2C
        if(mode == 1){
            if(node()->i2c.start_communication && !node()->i2c.address_sent)
            {
                node()->i2c.send_address();
            }

            if(node()->i2c.waitingForAck && !node()->i2c.ackReceived)
            {
                node()->i2c.wait_for_recieving_ACK();
            }

            if(node()->i2c.ackReceived && node()->i2c.byteCount < 3)
            {
                node()->i2c.recieve_3_bytes_of_data();
            }
        }

        // USART
        if(mode == 2){
            if(node()->usart.is_ready_to_send()) {
                node()->usart.write(0x22);
                std::cout << "[Microcontroller to Hard Disk] sends address :" << (int)0x22 << std::endl; 
            }
            
            //6
            if(!node()->usart.has_sensed_zero() && node()->usart.is_ready_to_send()) {
                node()->usart.set_sensed_zero();  
                if(node()->usart.has_sensed_zero()){
                    std::cout << "[Microcontroller from Hard Disk] sensed zero." << std::endl; 
                }
            }
            //7
            if((!node()->usart.has_received_byte()) && node()->usart.has_sensed_zero() && node()->usart.is_ready_to_send()) {
                data = node()->usart.read();
                std::cout << "[Microcontroller From Hard Disk] Received data is:" << (int)data << std::endl; 
            }
            //8
            if(!node()->usart.has_sensed_one() && node()->usart.has_received_byte() &&
                node()->usart.has_sensed_zero() && node()->usart.is_ready_to_send()) {
                node()->usart.set_sensed_one();
            }
            //9
            if(node()->usart.has_received_byte() && node()->usart.has_sensed_one()) {
                std::cout << "[Microcontroller Usart Hard Disk] is done!" << std::endl;
                node()->usart.reset();
            }
        }
        
        // I2CMux
        if(mode == 3)
        {
            static int selected_channel = 0;
            static bool channel_command_sent = false;
            static int received_byte_index = 0;
            static Byte received_sensor_packet[3]{};

            if(!channel_command_sent && node()->usart.is_ready_to_send())
            {
                Byte selected_channel_byte = static_cast<Byte>(selected_channel);
                node()->usart.write(selected_channel_byte);

                std::cout << "[Microcontroller] requesting sensor on channel " << selected_channel << std::endl;

                channel_command_sent = true;
            }

            if(channel_command_sent)
            {
                if(!node()->usart.has_sensed_zero())
                {
                    node()->usart.set_sensed_zero();
                }

                if(node()->usart.has_sensed_zero() &&
                !node()->usart.has_received_byte() &&
                node()->usart.has_byte_to_read())
                {
                    received_sensor_packet[received_byte_index] = node()->usart.read();
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
                    node()->usart.reset_receiver_only();
                    received_byte_index++;

                    if(received_byte_index == 3)
                    {
                        uint8_t low_byte = static_cast<UByte>(received_sensor_packet[0]);
                        uint8_t high_byte = static_cast<UByte>(received_sensor_packet[1]);
                        uint8_t received_checksum =
                            static_cast<UByte>(received_sensor_packet[2]);

                        uint8_t expected_checksum =
                            std::abs(static_cast<int>(low_byte) -
                                    static_cast<int>(high_byte));

                        if(received_checksum == expected_checksum)
                        {
                            uint16_t sensor_value = (static_cast<uint16_t>(high_byte) << 8) | low_byte;

                            std::cout <<"[Microcontroller] channel "<< selected_channel<<" value = "<< sensor_value<< " | checksum OK"<< std::endl;
                        }
                        else
                        {
                            std::cout <<"[Microcontroller] channel "<< selected_channel <<" checksum FAILED"
                            <<" | received="<< static_cast<int>(received_checksum)<< " expected="<< static_cast<int>(expected_checksum)<< std::endl;
                        }

                        received_byte_index = 0;
                        selected_channel = 1 - selected_channel;
                        channel_command_sent = false;
                    }
                }
            }
        }
        
        return 0;
    }
    private:
    Byte data;
    int mode {1};
};


#endif    // MICROCONTROLLER_H
