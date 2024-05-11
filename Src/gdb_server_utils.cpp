#include <gdb_server_utils.hpp>

//-----------------------------------------------------------------------------
//! Utility to give the value of a hex char

//! @param[in] ch  A character representing a hexadecimal digit. Done as -1,
//!                for consistency with other character routines, which can
//!                use -1 as EOF.

//! @return  The value of the hex character, or -1 if the character is
//!          invalid.
//-----------------------------------------------------------------------------
uint8_t GdbServerUtils::char2Hex(int c) {
  return ((c >= 'a') && (c <= 'f'))
    ? c - 'a' + 10
    : ((c >= '0') && (c <= '9'))
    ? c - '0'
    : ((c >= 'A') && (c <= 'F')) ? c - 'A' + 10 : -1;

}  // char2Hex ()

//-----------------------------------------------------------------------------
//! Utility mapping a value to hex character

//! @param[in] d  A hexadecimal digit. Any non-hex digit returns a NULL char
//-----------------------------------------------------------------------------
const char GdbServerUtils::hex2Char(uint8_t d) {
  static const char map[] =
    "0123456789abcdef"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

  return map[d];

}  // hex2Char ()

//-----------------------------------------------------------------------------
//! Convert a register to a hex digit string according to target endianness
//! @param[in]  val  The value to convert
//! @param[out] buf  The buffer for the text string
//-----------------------------------------------------------------------------
void GdbServerUtils::reg2Hex(uint32_t val, char* buf) {
  for (int n = 0; n < sizeof(uint32_t); n++) {
    // Swap nibbles
    buf[2 * n + 1] = hex2Char(val & 0xf);
    buf[2 * n] = hex2Char((val >> 4) & 0xf);
    val >>= 8;
  }

  buf[2 * sizeof(uint32_t)] = '\0';  // Null-termination
}  // reg2hex ()

//-----------------------------------------------------------------------------
//! Convert a hex digit string to a register value
//
//! @param[in] buf  The buffer with the hex string
//! @return  The value to convert
//-----------------------------------------------------------------------------
uint32_t GdbServerUtils::hex2Reg(char* buf, size_t nBytes) {
  uint32_t val = 0;  // The result
  for (int n = 0; n < 2 * nBytes; n++) {
    val = (val << 4) + char2Hex(buf[n]);
  }
  return val;
}  // hex2reg ()

//-----------------------------------------------------------------------------
//! Convert an ASCII character string to pairs of hex digits

//! Both source and destination are null terminated.

//! @param[out] dest  Buffer to store the hex digit pairs (null terminated)
//! @param[in]  src   The ASCII string (null terminated)                      */
//-----------------------------------------------------------------------------
void GdbServerUtils::ascii2Hex(char* dest, char* src) {
  int i;

  // Step through converting the source string
  for (i = 0; src[i] != '\0'; i++) {
    char ch = src[i];

    dest[i * 2] = hex2Char(ch >> 4 & 0xf);
    dest[i * 2 + 1] = hex2Char(ch & 0xf);
  }

  dest[i * 2] = '\0';

}  // ascii2hex ()

//-----------------------------------------------------------------------------
//! Convert pairs of hex digits to an ASCII character string

//! Both source and destination are null terminated.

//! @param[out] dest  The ASCII string (null terminated)
//! @param[in]  src   Buffer holding the hex digit pairs (null terminated)
//-----------------------------------------------------------------------------
void GdbServerUtils::hex2Ascii(char* dest, char* src) {
  int i;

  // Step through convering the source hex digit pairs
  for (i = 0; src[i * 2] != '\0' && src[i * 2 + 1] != '\0'; i++) {
    dest[i] =
      ((char2Hex(src[i * 2]) & 0xf) << 4) | (char2Hex(src[i * 2 + 1]) & 0xf);
  }

  dest[i] = '\0';

}  // hex2ascii ()

//-----------------------------------------------------------------------------
//! "Unescape" RSP binary data

//! '#', '$' and '}' are escaped by preceding them by '}' and oring with 0x20.

//! This function reverses that, modifying the data in place.

//! @param[in] buf  The array of bytes to convert
//! @para[in]  len   The number of bytes to be converted

//! @return  The number of bytes AFTER conversion
//-----------------------------------------------------------------------------
int GdbServerUtils::rspUnescape(char* buf, int len) {
  int fromOffset = 0;  // Offset to source char
  int toOffset = 0;    // Offset to dest char

  while (fromOffset < len) {
    // Is it escaped
    if ('}' == buf[fromOffset]) {
      fromOffset++;
      buf[toOffset] = buf[fromOffset] ^ 0x20;
    }
    else {
      buf[toOffset] = buf[fromOffset];
    }

    fromOffset++;
    toOffset++;
  }

  return toOffset;

}  // rspUnescape () */
