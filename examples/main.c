#include "../include/space_packet.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* CRC-16-CCITT (polynomial 0x1021, init 0xFFFF).
 * The CCSDS Space Packet primary header defines no checksum field; integrity
 * protection is an application-level concern. This function is a local helper
 * for the example — it is not part of the library public API.
 */
static uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int k = 0; k < 8; ++k)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

int main(void)
{
    /* Mission-defined Packet Data Field layout (example convention):
     *   Byte 0:   secondary header flags (bit 0 = CRC appended)
     *   Byte 1:   number of additional secondary header bytes (0 here)
     *   Bytes 2+: user payload
     *   Last 2:   CRC-16-CCITT over bytes 0..(N-3), big-endian
     *
     * The library copies this block verbatim; its internal structure is
     * entirely the application's responsibility.
     */
    const uint8_t sec_hdr[] = {0x01, 0x00}; /* flags: CRC present */
    const uint8_t user_payload[] = {'H', 'e', 'l', 'l', 'o', ' ', 'S', 'P'};

    uint8_t pkt_data[sizeof(sec_hdr) + sizeof(user_payload) + 2];
    size_t off = 0;
    memcpy(pkt_data + off, sec_hdr, sizeof(sec_hdr));
    off += sizeof(sec_hdr);
    memcpy(pkt_data + off, user_payload, sizeof(user_payload));
    off += sizeof(user_payload);
    uint16_t crc = crc16_ccitt(pkt_data, off);
    pkt_data[off++] = (uint8_t)(crc >> 8);
    pkt_data[off++] = (uint8_t)(crc & 0xFFu);

    sp_packet_t pkt;
    sp_packet_init(&pkt);
    sp_set_primary_header(&pkt,
                          0,     /* type: telemetry */
                          1,     /* secondary header present */
                          0x100, /* APID */
                          SP_SEQ_FLAG_UNSEGMENTED,
                          1 /* sequence count */);
    sp_set_data(&pkt, pkt_data, (uint16_t)off);

    uint8_t buf[256];
    size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
    if (n == 0)
    {
        printf("serialize failed\n");
        return 1;
    }

    printf("Serialized %zu bytes:\n", n);
    for (size_t i = 0; i < n; ++i)
        printf("%02X ", buf[i]);
    printf("\n");

    sp_packet_t parsed;
    if (!sp_packet_parse(&parsed, buf, n))
    {
        printf("parse failed\n");
        return 2;
    }

    /* Application-level CRC check. */
    if (parsed.data_len < 2)
    {
        printf("data too short to contain CRC\n");
        return 3;
    }
    size_t crc_area = parsed.data_len - 2;
    uint16_t crc_recv = ((uint16_t)parsed.data[crc_area] << 8) | parsed.data[crc_area + 1];
    uint16_t crc_calc = crc16_ccitt(parsed.data, crc_area);
    if (crc_recv != crc_calc)
    {
        printf("CRC mismatch (recv=0x%04X calc=0x%04X)\n", crc_recv, crc_calc);
        return 4;
    }

    /* Recover payload: skip 2-byte secondary header, drop 2-byte CRC. */
    size_t sec_total = (size_t)2 + parsed.data[1];
    const uint8_t *pay = parsed.data + sec_total;
    size_t pay_len = crc_area - sec_total;

    printf("APID=0x%03X seq_count=%u data_len=%u CRC OK\n",
           parsed.ph.apid,
           parsed.ph.seq_count,
           parsed.data_len);
    printf("Payload: ");
    fwrite(pay, 1, pay_len, stdout);
    printf("\n");

    return 0;
}
