/* Minimal SpacePacket header for embedded applications
 * Provides a tiny API to create, serialize and parse CCSDS-like packets.
 */
#ifndef SPACE_PACKET_H
#define SPACE_PACKET_H

#include <stddef.h>
#include <stdint.h>

/* Primary header is 6 bytes (CCSDS-like):
 * - bytes 0-1: version(3), type(1), sec_hdr(1), apid(11)
 * - bytes 2-3: seq_flags(2), seq_count(14)
 * - bytes 4-5: packet_length (payload length - 1)
 */

typedef struct {
  /* Primary header represented as bitfields (CCSDS-like) */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  unsigned version : 3;
  unsigned type : 1;
  unsigned sec_hdr_flag : 1; /* whether secondary header present */
  unsigned apid : 11;
#else
  unsigned apid : 11;
  unsigned sec_hdr_flag : 1;
  unsigned type : 1;
  unsigned version : 3;
#endif
  /* sequence control */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  unsigned seq_flags : 2;
  unsigned seq_count : 14;
#else
  unsigned seq_count : 14;
  unsigned seq_flags : 2;
#endif
  uint16_t packet_length;
} sp_primary_header_t;

typedef struct {
  sp_primary_header_t ph; /* primary header (bitwise representation) */

  const uint8_t *sec_hdr; /* secondary header pointer (if ph.bits.sec_hdr_flag),
                             as provided */
  uint16_t sec_hdr_len;   /* total secondary header length in bytes (>=2 when
                           * present)   Layout: byte0 = flags, byte1 =
                           * remaining_sec_len (n), followed by n bytes
                           */
  const uint8_t *payload; /* points into a buffer when parsed */
  uint16_t payload_len;
  /* If the secondary header flags (byte0) LSB is 1, a 16-bit CRC (big-endian)
   * is appended after payload. The CRC covers the secondary header and payload.
   */
} sp_packet_t;

/* Return required buffer size to serialize this packet (header + payload) */
size_t sp_packet_serialize_size(const sp_packet_t *pkt);

/* Serialize packet into `buf` of length `buf_len`. Returns bytes written or 0
 * on error. */
size_t sp_packet_serialize(const sp_packet_t *pkt, uint8_t *buf,
                           size_t buf_len);

/* Parse buffer into packet fields. Note: this does not allocate payload memory;
 * it sets `out->payload` to point into `buf`. Returns 1 on success, 0 on
 * failure.
 */
int sp_packet_parse(sp_packet_t *out, uint8_t *buf, size_t buf_len);

/* Utility: compute CRC-16-CCITT (polynomial 0x1021, init 0xFFFF). Returns
 * 16-bit CRC. */
uint16_t sp_crc16_ccitt(const uint8_t *data, size_t len);

#endif /* SPACE_PACKET_H */
