#ifndef CRC16_H
#define CRC16_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CRC16_CCITT_FALSE_INIT_VALUE    0xFFFFU

/**
 * @brief Calculate CRC16-CCITT-FALSE.
 *
 * Polynomial: 0x1021
 * Init value: 0xFFFF
 * RefIn: false
 * RefOut: false
 * XorOut: 0x0000
 *
 * @param data Data buffer.
 * @param len Data length.
 * @return CRC16 value.
 */
uint16_t Crc16_CcittFalse(const uint8_t *data, uint16_t len);

/**
 * @brief Update CRC16-CCITT-FALSE with one byte.
 *
 * This function is useful for streaming CRC calculation.
 *
 * @param crc Current CRC value.
 * @param byte New byte.
 * @return Updated CRC value.
 */
uint16_t Crc16_CcittFalseUpdateByte(uint16_t crc, uint8_t byte);

/**
 * @brief Update CRC16-CCITT-FALSE with buffer.
 *
 * @param crc Current CRC value.
 * @param data Data buffer.
 * @param len Data length.
 * @return Updated CRC value.
 */
uint16_t Crc16_CcittFalseUpdate(uint16_t crc, const uint8_t *data, uint16_t len);

/**
 * @brief Run CRC16 self-test.
 *
 * Standard test vector:
 * Input: "123456789"
 * Expected CRC16-CCITT-FALSE: 0x29B1
 *
 * @return 1 if test passed, otherwise 0.
 */
uint8_t Crc16_SelfTest(void);

#ifdef __cplusplus
}
#endif

#endif /* CRC16_H */