#include "../include/space_packet.h"
#include "cunit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_roundtrip_no_secheader(void) {
  uint8_t payload[] = {1, 2, 3, 4, 5};
  sp_packet_t pkt = {0};
  pkt.ph.apid = 0x01;
  pkt.ph.seq_count = 0x2;
  pkt.ph.sec_hdr_flag = 0;
  pkt.payload = payload;
  pkt.payload_len = sizeof(payload);

  size_t buf_len = sp_packet_serialize_size(&pkt);
  uint8_t *buf = (uint8_t *)malloc(buf_len);
  if (!buf)
    return 1;
  size_t n = sp_packet_serialize(&pkt, buf, buf_len);
  if (n == 0) {
    free(buf);
    return 1;
  }

  sp_packet_t parsed;
  int ok = sp_packet_parse(&parsed, buf, n);
  if (!ok) {
    free(buf);
    return 1;
  }

  ASSERT_EQ_INT(parsed.ph.apid, pkt.ph.apid);
  ASSERT_EQ_INT(parsed.ph.seq_count, pkt.ph.seq_count);
  ASSERT_EQ_INT(parsed.payload_len, pkt.payload_len);
  ASSERT_EQ_MEM(parsed.payload, pkt.payload, pkt.payload_len);

  free(buf);
  return 0;
}

int test_roundtrip_with_secheader_crc(void) {
  uint8_t payload[] = {10, 11, 12, 13};
  const uint8_t sec_hdr[] = {0x1, 0x0}; // flags LSB=1 -> CRC present

  sp_packet_t pkt = {0};
  pkt.ph.apid = 0x123;
  pkt.ph.seq_count = 0x3;
  pkt.ph.sec_hdr_flag = 1;
  pkt.sec_hdr = sec_hdr;
  pkt.sec_hdr_len = sizeof(sec_hdr);
  pkt.payload = payload;
  pkt.payload_len = sizeof(payload);

  size_t buf_len = sp_packet_serialize_size(&pkt);
  uint8_t *buf = (uint8_t *)malloc(buf_len);
  if (!buf)
    return 1;
  size_t n = sp_packet_serialize(&pkt, buf, buf_len);
  if (n == 0) {
    free(buf);
    return 1;
  }

  sp_packet_t parsed;
  int ok = sp_packet_parse(&parsed, buf, n);
  if (!ok) {
    free(buf);
    return 1;
  }

  ASSERT_EQ_INT(parsed.ph.apid, pkt.ph.apid);
  ASSERT_EQ_INT(parsed.payload_len, pkt.payload_len);
  ASSERT_EQ_MEM(parsed.payload, pkt.payload, pkt.payload_len);

  free(buf);
  return 0;
}

int test_crc_mismatch(void) {
  uint8_t payload[] = {20, 21, 22};
  const uint8_t sec_hdr[] = {0x1, 0x0};

  sp_packet_t pkt = {0};
  pkt.ph.apid = 0x7FF;
  pkt.ph.seq_count = 0x1;
  pkt.ph.sec_hdr_flag = 1;
  pkt.sec_hdr = sec_hdr;
  pkt.sec_hdr_len = sizeof(sec_hdr);
  pkt.payload = payload;
  pkt.payload_len = sizeof(payload);

  size_t buf_len = sp_packet_serialize_size(&pkt);
  uint8_t *buf = (uint8_t *)malloc(buf_len);
  if (!buf)
    return 1;
  size_t n = sp_packet_serialize(&pkt, buf, buf_len);
  if (n == 0) {
    free(buf);
    return 1;
  }

  // Corrupt a payload byte (within payload region)
  size_t corrupt_index = 6 + pkt.sec_hdr_len + 0;
  if (corrupt_index < n)
    buf[corrupt_index] ^= 0xFF;

  sp_packet_t parsed;
  int ok = sp_packet_parse(&parsed, buf, n);
  free(buf);
  return ok ? 1 : 0; // expect parse to fail (ok==0)
}

int test_malformed_short_buffer(void) {
  uint8_t tiny[] = {0x00, 0x01};
  sp_packet_t parsed;
  int ok = sp_packet_parse(&parsed, tiny, sizeof(tiny));
  return ok ? 1 : 0; // expect parse to fail
}

int main(void) {
  RUN_TEST(test_roundtrip_no_secheader);
  RUN_TEST(test_roundtrip_with_secheader_crc);
  RUN_TEST(test_crc_mismatch);
  RUN_TEST(test_malformed_short_buffer);

  if (cunit_overall_failures == 0) {
    printf("ALL TESTS PASSED\n");
    return 0;
  } else {
    printf("%d TEST(S) FAILED\n", cunit_overall_failures);
    return 1;
  }
}
