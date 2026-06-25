/**
 * @file space_packet.c
 * @brief CCSDS Space Packet Protocol — serialiser, parser and header helpers.
 */
#include "../include/space_packet.h"

#include <string.h>

void sp_packet_init(sp_packet_t *pkt)
{
    if (!pkt)
        return;
    memset(pkt, 0, sizeof(*pkt));
}

void sp_set_primary_header(sp_packet_t *pkt,
                           uint8_t type,
                           uint8_t sec_hdr_flag,
                           uint16_t apid,
                           sp_seq_flag_t seq_flags,
                           uint16_t seq_count)
{
    if (!pkt)
        return;
    pkt->ph.version = 0;
    pkt->ph.type = (unsigned)(type & 0x1u);
    pkt->ph.sec_hdr_flag = (unsigned)(sec_hdr_flag ? 1u : 0u);
    pkt->ph.apid = (unsigned)(apid & 0x07FFu);
    pkt->ph.seq_flags = (sp_seq_flag_t)((unsigned)(seq_flags) & 0x3u);
    pkt->ph.seq_count = (unsigned)(seq_count & 0x3FFFu);
}

void sp_set_data(sp_packet_t *pkt, const uint8_t *data, uint16_t data_len)
{
    if (!pkt)
        return;
    pkt->data = data;
    pkt->data_len = data_len;
}

size_t sp_packet_serialize_size(const sp_packet_t *pkt)
{
    if (!pkt || pkt->data_len == 0)
        return 0;
    return (size_t)6 + pkt->data_len;
}

size_t sp_packet_serialize(const sp_packet_t *pkt, uint8_t *buf, size_t buf_len)
{
    if (!pkt || !buf || !pkt->data || pkt->data_len == 0)
        return 0;

    size_t need = (size_t)6 + pkt->data_len;
    if (buf_len < need)
        return 0;

    /* Version bits are always 000 (CCSDS 133.0-B-2 §4.1.3.2). */
    uint16_t first = (uint16_t)(((pkt->ph.type & 0x1u) << 12) |
                                ((pkt->ph.sec_hdr_flag & 0x1u) << 11) | (pkt->ph.apid & 0x07FFu));
    uint16_t second =
        (uint16_t)(((unsigned)(pkt->ph.seq_flags & 0x3u) << 14) | (pkt->ph.seq_count & 0x3FFFu));
    uint16_t length = (uint16_t)(pkt->data_len - 1u);

    buf[0] = (uint8_t)(first >> 8);
    buf[1] = (uint8_t)(first & 0xFFu);
    buf[2] = (uint8_t)(second >> 8);
    buf[3] = (uint8_t)(second & 0xFFu);
    buf[4] = (uint8_t)(length >> 8);
    buf[5] = (uint8_t)(length & 0xFFu);

    memcpy(&buf[6], pkt->data, pkt->data_len);

    return need;
}

int sp_packet_parse(sp_packet_t *out, const uint8_t *buf, size_t buf_len)
{
    if (!out || !buf)
        return 0;
    if (buf_len < 6)
        return 0;

    uint16_t first = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t second = ((uint16_t)buf[2] << 8) | buf[3];
    uint16_t length_field = ((uint16_t)buf[4] << 8) | buf[5];

    out->ph.version = (unsigned)((first >> 13) & 0x7u);
    out->ph.type = (unsigned)((first >> 12) & 0x1u);
    out->ph.sec_hdr_flag = (unsigned)((first >> 11) & 0x1u);
    out->ph.apid = (unsigned)(first & 0x07FFu);
    out->ph.seq_flags = (sp_seq_flag_t)((second >> 14) & 0x3u);
    out->ph.seq_count = (unsigned)(second & 0x3FFFu);
    out->ph.packet_length = length_field;

    uint16_t data_len = (uint16_t)(length_field + 1u);

    if (buf_len < (size_t)6 + data_len)
        return 0;

    out->data = &buf[6];
    out->data_len = data_len;

    return 1;
}
