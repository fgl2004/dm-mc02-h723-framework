#include "crc16.h"

#include <stddef.h>

#define CRC16_CCITT_FALSE_POLY    0x1021U

uint16_t Crc16_CcittFalseUpdateByte(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)((uint16_t)byte << 8);

    for (uint8_t i = 0U; i < 8U; i++)
    {
        if ((crc & 0x8000U) != 0U)
        {
            crc = (uint16_t)((crc << 1) ^ CRC16_CCITT_FALSE_POLY);
        }
        else
        {
            crc = (uint16_t)(crc << 1);
        }
    }

    return crc;
}

uint16_t Crc16_CcittFalseUpdate(uint16_t crc, const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U))
    {
        return crc;
    }

    for (uint16_t i = 0U; i < len; i++)
    {
        crc = Crc16_CcittFalseUpdateByte(crc, data[i]);
    }

    return crc;
}

uint16_t Crc16_CcittFalse(const uint8_t *data, uint16_t len)
{
    return Crc16_CcittFalseUpdate(CRC16_CCITT_FALSE_INIT_VALUE, data, len);
}

uint8_t Crc16_SelfTest(void)
{
    static const uint8_t test_data[] = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };

    uint16_t crc;

    crc = Crc16_CcittFalse(test_data, (uint16_t)sizeof(test_data));

    return (crc == 0x29B1U) ? 1U : 0U;
}