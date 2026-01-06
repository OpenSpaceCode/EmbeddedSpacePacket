#include "../include/space_packet.h"
#include <string.h>
#include <stdlib.h>

/* Minimal serializer/parser for a 6-byte primary header + payload.
 * Uses big-endian network byte order for header fields.
 */

size_t sp_packet_serialize_size(const sp_packet_t *pkt) {
    if (!pkt) return 0;
    size_t total = 6;
    if (pkt->ph.sec_hdr_flag) total += pkt->sec_hdr_len;
    total += pkt->payload_len;
    /* if secondary header flags request CRC, include 2 bytes
     * (we can't inspect sec_hdr here without checking length) */
    if (pkt->ph.sec_hdr_flag && pkt->sec_hdr_len >= 2) {
        if (pkt->sec_hdr[0] & 0x1) total += 2;
    }
    return total;
}

size_t sp_packet_serialize(const sp_packet_t *pkt, uint8_t *buf, size_t buf_len) {
    if (!pkt || !buf) return 0;
    size_t need = sp_packet_serialize_size(pkt);
    if (buf_len < need) return 0;

    uint16_t first = 0;
    uint16_t second = 0;
    uint16_t length_field = 0;

    /* compose first and second words from bitfields */
    first = (uint16_t)(((pkt->ph.version & 0x7) << 13) |
                       ((pkt->ph.type & 0x1) << 12) |
                       ((pkt->ph.sec_hdr_flag & 0x1) << 11) |
                       (pkt->ph.apid & 0x07FF));

    second = (uint16_t)((((pkt->ph.seq_flags & 0x3) << 14) | (pkt->ph.seq_count & 0x3FFF)));

    /* Compute total length (secondary header + payload [+crc]) */
    uint16_t total_len = 0;
    if (pkt->ph.sec_hdr_flag) total_len += pkt->sec_hdr_len;
    total_len += pkt->payload_len;
    int crc_present = 0;
    if (pkt->ph.sec_hdr_flag && pkt->sec_hdr_len >= 2) {
        crc_present = (pkt->sec_hdr[0] & 0x1) ? 1 : 0;
    }
    if (crc_present) total_len += 2;

    if (total_len == 0) length_field = 0;
    else length_field = (uint16_t)(total_len - 1);

    /* write big-endian */
    buf[0] = (uint8_t)((first >> 8) & 0xFF);
    buf[1] = (uint8_t)(first & 0xFF);
    buf[2] = (uint8_t)((second >> 8) & 0xFF);
    buf[3] = (uint8_t)(second & 0xFF);
    buf[4] = (uint8_t)((length_field >> 8) & 0xFF);
    buf[5] = (uint8_t)(length_field & 0xFF);

    size_t off = 6;
    if (pkt->ph.sec_hdr_flag && pkt->sec_hdr && pkt->sec_hdr_len) {
        memcpy(&buf[off], pkt->sec_hdr, pkt->sec_hdr_len);
        off += pkt->sec_hdr_len;
    }
    if (pkt->payload_len && pkt->payload) {
        memcpy(&buf[off], pkt->payload, pkt->payload_len);
        off += pkt->payload_len;
    }

    if (crc_present) {
        /* compute CRC over secondary header and payload */
        size_t crc_area_len = (pkt->ph.sec_hdr_flag ? pkt->sec_hdr_len : 0) + pkt->payload_len;
        uint16_t crc = sp_crc16_ccitt(&buf[6], crc_area_len);
        buf[off++] = (uint8_t)((crc >> 8) & 0xFF);
        buf[off++] = (uint8_t)(crc & 0xFF);
    }

    return need;
}

int sp_packet_parse(sp_packet_t *out, uint8_t *buf, size_t buf_len) {
    if (!out || !buf) return 0;
    if (buf_len < 6) return 0;

    uint16_t first = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t second = ((uint16_t)buf[2] << 8) | buf[3];
    uint16_t length_field = ((uint16_t)buf[4] << 8) | buf[5];

    out->ph.version = (first >> 13) & 0x7;
    out->ph.type = (first >> 12) & 0x1;
    out->ph.sec_hdr_flag = (first >> 11) & 0x1;
    out->ph.apid = first & 0x07FF;
    out->ph.seq_flags = (second >> 14) & 0x3;
    out->ph.seq_count = second & 0x3FFF;
    out->ph.packet_length = length_field;

    uint16_t total_len = (uint16_t)(length_field + 1);

    size_t off = 6;
    if (out->ph.sec_hdr_flag) {
        /* need at least 2 bytes for secondary header to read its length */
        if (buf_len < off + 2) return 0;
        uint8_t flags = buf[off];
        uint8_t rem_len = buf[off + 1];
        uint16_t sec_len = (uint16_t)(2 + rem_len);
        if (buf_len < off + sec_len) return 0;
        out->sec_hdr = &buf[off];
        out->sec_hdr_len = sec_len;
        off += sec_len;

        int crc_present = (flags & 0x1) ? 1 : 0;

        uint16_t payload_len_calc = 0;
        if (crc_present) {
            if (total_len < sec_len + 2) return 0; /* impossible */
            payload_len_calc = (uint16_t)(total_len - sec_len - 2);
            if (buf_len < off + payload_len_calc + 2) return 0;
            out->payload_len = payload_len_calc;
            out->payload = &buf[off];

            /* verify CRC */
            size_t crc_area_len = sec_len + out->payload_len;
            uint16_t crc_calc = sp_crc16_ccitt(&buf[6], crc_area_len);
            uint16_t crc_recv = ((uint16_t)buf[off + out->payload_len] << 8) | buf[off + out->payload_len + 1];
            if (crc_calc != crc_recv) return 0;
        } else {
            if (total_len < sec_len) return 0;
            payload_len_calc = (uint16_t)(total_len - sec_len);
            if (buf_len < off + payload_len_calc) return 0;
            out->payload_len = payload_len_calc;
            out->payload = &buf[off];
        }

    } else {
        /* no secondary header; CRC not supported */
        out->sec_hdr = NULL;
        out->sec_hdr_len = 0;
        out->payload_len = total_len;
        if (buf_len < off + out->payload_len) return 0;
        out->payload = &buf[off];
    }

    return 1;
}

uint16_t sp_crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int k = 0; k < 8; ++k) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc & 0xFFFF;
}
