#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../include/space_packet.h"

int main(void) {
    const uint8_t payload[] = { 'H','e','l','l','o',' ', 'S','P' };
    sp_packet_t pkt = {0};
    pkt.ph.apid = 0x100;
    pkt.ph.seq_count = 1;
    /* Example with a minimal secondary header containing flags and zero extra bytes.
     * flags byte LSB=1 requests CRC; byte1=0 indicates zero remaining sec header bytes.
     */
    const uint8_t sec_hdr[] = { 0x1, 0x0 };
    pkt.ph.sec_hdr_flag = 1;
    pkt.sec_hdr = sec_hdr;
    pkt.sec_hdr_len = sizeof(sec_hdr);
    pkt.payload = payload;
    pkt.payload_len = sizeof(payload);

    uint8_t buf[256];
    size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
    if (n == 0) {
        printf("serialize failed\n");
        return 1;
    }

    printf("Serialized %zu bytes:\n", n);
    for (size_t i = 0; i < n; ++i) printf("%02X ", buf[i]);
    printf("\n");

    sp_packet_t parsed;
    if (!sp_packet_parse(&parsed, buf, n)) {
        printf("parse failed\n");
        return 2;
    }

        printf("Parsed APID=0x%03X seq=%u payload_len=%u\n",
               parsed.ph.apid, parsed.ph.seq_count, parsed.payload_len);
    printf("Payload as ASCII: ");
    fwrite(parsed.payload, 1, parsed.payload_len, stdout);
    printf("\n");

    return 0;
}
