#ifndef __FIREFLY_UNITS_H__
#define __FIREFLY_UNITS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


///////////////////////////////
// Unit conversion

// The maximum length of a 256-bit value
#define FFX_BIGINT_LENGTH              (32)

// The maximum length of a 256-bit value, including:
//   - 78 bytes, for numeric content
//   - 1 byte, for any sign
//   - 1 byte, for any decimal point (decimals == 0 has no dot)
//   - 1 bytes NULL termination
#define FFX_BIGINT_STRING_LENGTH       (78 + 1 + 1 + 1)


// Returns 0 on error, otherwise the string length (excluding the NULL)
size_t ffx_units_formatValue(char *output, uint8_t *data, size_t length,
  uint8_t decimals);

size_t ffx_units_parseValue(uint8_t *output, char *text, uint8_t decimals);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __FIREFLY_UNITS_H__ */
