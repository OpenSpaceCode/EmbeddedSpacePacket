/* Minimal Space Packet implementation for embedded applications.
 * Implements the CCSDS Space Packet Protocol primary header (CCSDS 133.0-B-2).
 *
 * The Packet Data Field (everything after the 6-byte primary header) is treated
 * as opaque, application-owned bytes. Its internal layout — secondary header,
 * user data, any integrity check — is mission-specific and outside this library.
 */
#ifndef SPACE_PACKET_H
#define SPACE_PACKET_H

#include <stddef.h>
#include <stdint.h>

/* CCSDS 133.0-B-2 §4.1.3.4.2: Sequence Flags wire encoding. */
typedef enum
{
    SP_SEQ_FLAG_CONTINUING_SEGMENT = 0,
    SP_SEQ_FLAG_FIRST_SEGMENT      = 1,
    SP_SEQ_FLAG_LAST_SEGMENT       = 2,
    SP_SEQ_FLAG_UNSEGMENTED        = 3
} sp_seq_flag_t;

/* Primary header — 6 bytes, big-endian on the wire (CCSDS 133.0-B-2 §4.1.3):
 *  bits  0-2:  Packet Version Number (always 000)
 *  bit   3:    Packet Type (0 = TM/telemetry, 1 = TC/telecommand)
 *  bit   4:    Secondary Header Flag
 *  bits  5-15: APID (11 bits)
 *  bits 16-17: Sequence Flags
 *  bits 18-31: Packet Sequence Count (14 bits)
 *  bits 32-47: Packet Data Length = (Packet Data Field octets) - 1
 */
typedef struct
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    unsigned version : 3;
    unsigned type : 1;
    unsigned sec_hdr_flag : 1;
    unsigned apid : 11;
    sp_seq_flag_t seq_flags : 2;
    unsigned seq_count : 14;
#else
    unsigned apid : 11;
    unsigned sec_hdr_flag : 1;
    unsigned type : 1;
    unsigned version : 3;
    unsigned seq_count : 14;
    sp_seq_flag_t seq_flags : 2;
#endif
    uint16_t packet_length;
} sp_primary_header_t;

/* A Space Packet: primary header + Packet Data Field.
 * `data` points to the caller-owned Packet Data Field; the library copies it
 * verbatim into the wire buffer. The caller is responsible for any secondary
 * header, user data layout, and optional integrity checks within that field.
 */
typedef struct
{
    sp_primary_header_t ph;
    const uint8_t *data;
    uint16_t data_len;
} sp_packet_t;

/* Zero-initialize a packet structure. */
void sp_packet_init(sp_packet_t *pkt);

/* Set primary header fields. Version is always forced to 0 (CCSDS §4.1.3.2).
 * All other values are masked to their valid bit widths. */
void sp_set_primary_header(sp_packet_t *pkt,
                           uint8_t type,
                           uint8_t sec_hdr_flag,
                           uint16_t apid,
                           sp_seq_flag_t seq_flags,
                           uint16_t seq_count);

/* Bind the Packet Data Field (application-owned memory, not copied here). */
void sp_set_data(sp_packet_t *pkt, const uint8_t *data, uint16_t data_len);

/* Return the number of bytes sp_packet_serialize will write for this packet,
 * or 0 if the packet is invalid (NULL or data_len == 0). */
size_t sp_packet_serialize_size(const sp_packet_t *pkt);

/* Serialize packet into buf[0..buf_len). Returns bytes written, or 0 on error.
 * Fails if pkt or buf is NULL, buf is too small, data is NULL, or data_len is
 * 0 (Packet Data Field must contain at least one octet, CCSDS §4.1.4.1.2). */
size_t sp_packet_serialize(const sp_packet_t *pkt, uint8_t *buf, size_t buf_len);

/* Parse a wire-format Space Packet from buf[0..buf_len). On success, out->data
 * points into buf (zero-copy; keep buf alive as long as out is used).
 * Returns 1 on success, 0 on failure (NULL args, short buffer). */
int sp_packet_parse(sp_packet_t *out, const uint8_t *buf, size_t buf_len);

#endif /* SPACE_PACKET_H */
