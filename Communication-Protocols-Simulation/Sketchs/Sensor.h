#ifndef SENSOR_H
#define SENSOR_H

#include <CPS4042/Hardwares/Sensors/VL530X.h>
#include <CPS4042/Sketchs/AbstractSketch.h>

#include <iostream>

class Sensor : public AbstractSketch<Sensors::Vl530x>
{
public:
    explicit Sensor(Sensors::Vl530x* sensor_node,
                    int min_value = 0,
                    int max_value = 4000) :
        AbstractSketch<Sensors::Vl530x> {sensor_node},
        min_value {min_value},
        max_value {max_value}
    {
        sensor_node->i2c.set_range(this->min_value, this->max_value);
    }

    std::int32_t setup(Sensors::Vl530x::Gpio&) override
    {
        std::cout << "[Sensor] ready with value range "
                  << min_value
                  << " to "
                  << max_value
                  << std::endl;
        
        // node()->i2c.init(0x29);
        std::cout << "vl530x setup completed." << std::endl;

        return 0;
    }

    std::int32_t loop(Sensors::Vl530x::Gpio&) override
    {
        if(!node()->i2c.addressMatched)
        {
            node()->i2c.recieve_and_check_address();
        }

        if(node()->i2c.ackSent && !node()->i2c.dataSent)
        {
            node()->i2c.send_3_bytes_of_data();

            if(node()->i2c.dataSent)
            {
                node()->i2c.reset_transaction();
            }
        }

        return 0;
    }

private:
    int min_value {0};
    int max_value {4000};
};

#endif // SENSOR_H