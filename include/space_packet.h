/**
 * @file space_packet.h
 * @brief CCSDS Space Packet Protocol public API (CCSDS 133.0-B-2).
 *
 * Implements the 6-byte primary header. The Packet Data Field is treated as an
 * opaque, application-owned byte sequence; its internal layout is mission-specific.
 */
#ifndef SPACE_PACKET_H
#define SPACE_PACKET_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Sequence Flags wire encoding (CCSDS 133.0-B-2 §4.1.3.4.2).
 *
 * Enum values equal the 2-bit wire pattern directly.
 */
typedef enum
{
    SP_SEQ_FLAG_CONTINUING_SEGMENT = 0, /**< Continuation segment of a larger packet group. */
    SP_SEQ_FLAG_FIRST_SEGMENT = 1,      /**< First segment of a larger packet group. */
    SP_SEQ_FLAG_LAST_SEGMENT = 2,       /**< Last segment of a larger packet group. */
    SP_SEQ_FLAG_UNSEGMENTED = 3         /**< Standalone, unsegmented packet. */
} sp_seq_flag_t;

typedef enum
{
    SP_PACKET_TYPE_TM = 0, /**< Telemetry (TM) */
    SP_PACKET_TYPE_TC = 1, /**< Telecommand (TC) */
} sp_packet_type_t;

/**
 * @brief Decoded CCSDS Space Packet primary header fields (CCSDS 133.0-B-2 §4.1.3).
 *
 * @note Do not serialise this struct directly; use sp_packet_serialize().
 */
typedef struct
{
    unsigned version : 3;        /**< Packet Version Number — always 0 on transmit (§4.1.3.2). */
    sp_packet_type_t type : 1;   /**< Packet Type (see ::sp_packet_type_t). */
    unsigned sec_hdr_flag : 1;   /**< Secondary Header Flag: 1 if a secondary header is present. */
    unsigned apid : 11;          /**< Application Process Identifier (11 bits). */
    sp_seq_flag_t seq_flags : 2; /**< Sequence Flags (see ::sp_seq_flag_t). */
    unsigned seq_count : 14;     /**< Packet Sequence Count, modulo-16384 per APID (§4.1.3.4.3). */
    uint16_t packet_length;      /**< Raw Packet Data Length field: (data octets) − 1 (§4.1.3.5). */
} sp_primary_header_t;

/**
 * @brief A Space Packet: primary header plus Packet Data Field.
 *
 * @note @p data points into caller-owned memory; the library neither copies nor frees it.
 */
typedef struct
{
    sp_primary_header_t ph; /**< Decoded primary header fields. */
    const uint8_t
        *data; /**< Packet Data Field — mission-defined layout (secondary header + user data). */
    uint16_t data_len; /**< Packet Data Field length in bytes. */
} sp_packet_t;

/**
 * @brief Zero-initialise a packet structure.
 *
 * @param[out] pkt Packet to initialise. No-op if NULL.
 */
void sp_packet_init(sp_packet_t *pkt);

/**
 * @brief Set primary header fields.
 *
 * Version is always forced to 0 (§4.1.3.2). All other values are masked to
 * their valid bit widths.
 *
 * @param[out] pkt          Target packet. No-op if NULL.
 * @param[in]  type         Packet Type (see ::sp_packet_type_t).
 * @param[in]  sec_hdr_flag Secondary Header Flag (0 or 1).
 * @param[in]  apid         APID — excess bits beyond 11 are masked.
 * @param[in]  seq_flags    Sequence Flags.
 * @param[in]  seq_count    Packet Sequence Count — excess bits beyond 14 are masked.
 */
void sp_set_primary_header(sp_packet_t *pkt,
                           sp_packet_type_t type,
                           uint8_t sec_hdr_flag,
                           uint16_t apid,
                           sp_seq_flag_t seq_flags,
                           uint16_t seq_count);

/**
 * @brief Bind the Packet Data Field.
 *
 * @param[out] pkt      Target packet. No-op if NULL.
 * @param[in]  data     Packet Data Field (not copied; caller keeps memory alive).
 * @param[in]  data_len Length of @p data in bytes.
 */
void sp_set_data(sp_packet_t *pkt, const uint8_t *data, uint16_t data_len);

/**
 * @brief Return the serialised size of a packet.
 *
 * @param[in] pkt Packet to measure.
 * @return 6 + data_len, or 0 if @p pkt is NULL or data_len is 0.
 */
size_t sp_packet_serialize_size(const sp_packet_t *pkt);

/**
 * @brief Serialise a Space Packet into a caller-supplied buffer.
 *
 * @param[in]  pkt     Packet to serialise.
 * @param[out] buf     Output buffer.
 * @param[in]  buf_len Buffer capacity in bytes.
 * @return Bytes written, or 0 on error (NULL args, empty data, or buffer too small).
 */
size_t sp_packet_serialize(const sp_packet_t *pkt, uint8_t *buf, size_t buf_len);

/**
 * @brief Parse a wire-format Space Packet.
 *
 * On success, @p out->data points into @p buf (zero-copy). Keep @p buf alive
 * as long as the parsed packet is in use.
 *
 * @param[out] out     Decoded packet.
 * @param[in]  buf     Wire buffer to parse.
 * @param[in]  buf_len Buffer length in bytes.
 * @return 1 on success, 0 on failure (NULL args or buffer shorter than declared data length).
 */
int sp_packet_parse(sp_packet_t *out, const uint8_t *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* SPACE_PACKET_H */
