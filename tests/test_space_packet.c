#include "../include/space_packet.h"
#include "cunit.h"
#include "test_runners.h"

#include <stdlib.h>
#include <string.h>

static int test_roundtrip_no_secheader(void)
{
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
    if (n == 0)
    {
        free(buf);
        return 1;
    }

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
    {
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

static int test_roundtrip_with_secheader_crc(void)
{
    uint8_t payload[] = {10, 11, 12, 13};
    const uint8_t sec_hdr[] = {0x1, 0x0}; // flags LSB=1 -> CRC present

    sp_packet_t pkt = {0};
    pkt.ph.apid = 0x123;
    pkt.ph.seq_flags = SP_SEQ_FLAG_UNSEGMENTED;
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
    if (n == 0)
    {
        free(buf);
        return 1;
    }

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
    {
        free(buf);
        return 1;
    }

    ASSERT_EQ_INT(parsed.ph.apid, pkt.ph.apid);
    ASSERT_EQ_INT(parsed.ph.seq_count, pkt.ph.seq_count);
    ASSERT_EQ_INT(parsed.ph.seq_flags, pkt.ph.seq_flags);
    ASSERT_EQ_INT(parsed.ph.sec_hdr_flag, pkt.ph.sec_hdr_flag);
    ASSERT_EQ_INT(parsed.sec_hdr_len, pkt.sec_hdr_len);
    ASSERT_EQ_MEM(parsed.sec_hdr, pkt.sec_hdr, pkt.sec_hdr_len);
    ASSERT_EQ_INT(parsed.payload_len, pkt.payload_len);
    ASSERT_EQ_MEM(parsed.payload, pkt.payload, pkt.payload_len);

    free(buf);
    return 0;
}

static int test_crc_mismatch(void)
{
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
    if (n == 0)
    {
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

static int test_malformed_short_buffer(void)
{
    uint8_t tiny[] = {0x00, 0x01};
    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, tiny, sizeof(tiny));
    return ok ? 1 : 0; // expect parse to fail
}

static int test_highlevel_api(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);

    const uint8_t payload[] = {'T', 'E', 'S', 'T'};
    const uint8_t sec_hdr[] = {0x01, 0x00};

    sp_set_primary_header(&pkt, 0, 0, 1, 0x456, SP_SEQ_FLAG_FIRST_SEGMENT, 42);
    sp_set_secondary_header(&pkt, sec_hdr, sizeof(sec_hdr));
    sp_set_payload(&pkt, payload, sizeof(payload));

    uint8_t buf[256];
    size_t n = sp_packet_build_frame(&pkt, buf, sizeof(buf));
    if (n == 0)
        return 1;

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
        return 1;

    ASSERT_EQ_INT(parsed.ph.apid, 0x456);
    ASSERT_EQ_INT(parsed.ph.seq_count, 42);
    ASSERT_EQ_INT(parsed.ph.seq_flags, SP_SEQ_FLAG_FIRST_SEGMENT);
    ASSERT_EQ_INT(parsed.payload_len, sizeof(payload));
    ASSERT_EQ_MEM(parsed.payload, payload, sizeof(payload));

    return 0;
}

static int test_null_pointers(void)
{
    sp_packet_t pkt = {0};
    uint8_t buf[64];

    size_t sz = sp_packet_serialize_size(NULL);
    if (sz != 0)
        return 1;

    sz = sp_packet_serialize(NULL, buf, sizeof(buf));
    if (sz != 0)
        return 1;

    sz = sp_packet_serialize(&pkt, NULL, sizeof(buf));
    if (sz != 0)
        return 1;

    int ok = sp_packet_parse(NULL, buf, sizeof(buf));
    if (ok)
        return 1;

    ok = sp_packet_parse(&pkt, NULL, sizeof(buf));
    if (ok)
        return 1;

    sp_packet_init(NULL);
    sp_set_primary_header(NULL, 0, 0, 0, 0, SP_SEQ_FLAG_UNSEGMENTED, 0);
    sp_set_secondary_header(NULL, buf, 10);
    sp_set_payload(NULL, buf, 10);

    return 0;
}

static int test_secheader_no_crc(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);

    const uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    const uint8_t sec_hdr[] = {0x00, 0x00}; // flags LSB=0 -> no CRC

    sp_set_primary_header(&pkt, 0, 0, 1, 0x200, SP_SEQ_FLAG_UNSEGMENTED, 10);
    sp_set_secondary_header(&pkt, sec_hdr, sizeof(sec_hdr));
    sp_set_payload(&pkt, payload, sizeof(payload));

    uint8_t buf[256];
    size_t n = sp_packet_build_frame(&pkt, buf, sizeof(buf));
    if (n == 0)
        return 1;

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
        return 1;

    ASSERT_EQ_INT(parsed.ph.apid, 0x200);
    ASSERT_EQ_INT(parsed.payload_len, sizeof(payload));
    ASSERT_EQ_MEM(parsed.payload, payload, sizeof(payload));

    return 0;
}

static int test_sequence_flags(void)
{
    const sp_seq_flag_t flags[] = {SP_SEQ_FLAG_UNSEGMENTED,
                                   SP_SEQ_FLAG_FIRST_SEGMENT,
                                   SP_SEQ_FLAG_CONTINUING_SEGMENT,
                                   SP_SEQ_FLAG_LAST_SEGMENT};

    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
    {
        sp_packet_t pkt;
        sp_packet_init(&pkt);

        const uint8_t payload[] = {(uint8_t)i};
        sp_set_primary_header(&pkt, 0, 0, 0, 0x100, flags[i], (uint16_t)i);
        sp_set_payload(&pkt, payload, sizeof(payload));

        uint8_t buf[256];
        size_t n = sp_packet_build_frame(&pkt, buf, sizeof(buf));
        if (n == 0)
            return 1;

        sp_packet_t parsed;
        int ok = sp_packet_parse(&parsed, buf, n);
        if (!ok)
            return 1;

        ASSERT_EQ_INT(parsed.ph.seq_flags, flags[i]);
    }

    return 0;
}

static int test_version_and_type(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);

    const uint8_t payload[] = {0x99};

    sp_set_primary_header(&pkt, 7, 1, 0, 0x7FF, SP_SEQ_FLAG_UNSEGMENTED, 0x3FFF);
    sp_set_payload(&pkt, payload, sizeof(payload));

    uint8_t buf[256];
    size_t n = sp_packet_build_frame(&pkt, buf, sizeof(buf));
    if (n == 0)
        return 1;

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
        return 1;

    ASSERT_EQ_INT(parsed.ph.version, 7);
    ASSERT_EQ_INT(parsed.ph.type, 1);
    ASSERT_EQ_INT(parsed.ph.apid, 0x7FF);
    ASSERT_EQ_INT(parsed.ph.seq_count, 0x3FFF);

    return 0;
}

static int test_buffer_too_small(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);

    const uint8_t payload[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    sp_set_primary_header(&pkt, 0, 0, 0, 0x100, SP_SEQ_FLAG_UNSEGMENTED, 1);
    sp_set_payload(&pkt, payload, sizeof(payload));

    uint8_t buf[8]; // too small
    size_t n = sp_packet_build_frame(&pkt, buf, sizeof(buf));

    return (n == 0) ? 0 : 1; // expect failure
}

static int test_empty_payload(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);

    sp_set_primary_header(&pkt, 0, 0, 0, 0x100, SP_SEQ_FLAG_UNSEGMENTED, 1);
    sp_set_payload(&pkt, NULL, 0);

    uint8_t buf[256];
    size_t n = sp_packet_build_frame(&pkt, buf, sizeof(buf));
    if (n == 0)
        return 1;

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    return ok ? 1 : 0;
}

static int test_crc_computation(void)
{
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t crc = sp_crc16_ccitt(data, sizeof(data));

    uint16_t crc2 = sp_crc16_ccitt(data, sizeof(data));
    ASSERT_EQ_INT(crc, crc2);

    uint16_t crc_empty = sp_crc16_ccitt(data, 0);
    ASSERT_EQ_INT(crc_empty, 0xFFFF);

    return 0;
}

static int test_parse_short_secheader(void)
{
    uint8_t buf[8];
    buf[0] = 0x08;
    buf[1] = 0x00;
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = 0x00;
    buf[5] = 0x01;
    buf[6] = 0x00;

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, 7);

    return ok ? 1 : 0; // expect parse to fail
}

static int test_parse_short_payload_crc(void)
{
    uint8_t buf[10];
    buf[0] = 0x08;
    buf[1] = 0x00;
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = 0x00;
    buf[5] = 0x05;
    buf[6] = 0x01;
    buf[7] = 0x00;

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, 8);

    return ok ? 1 : 0; // expect parse to fail
}

static int test_secheader_extended(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);

    const uint8_t payload[] = {0xDE, 0xAD};
    const uint8_t sec_hdr[] = {0x00, 0x03, 0xAA, 0xBB, 0xCC};

    sp_set_primary_header(&pkt, 1, 0, 1, 0x300, SP_SEQ_FLAG_CONTINUING_SEGMENT, 99);
    sp_set_secondary_header(&pkt, sec_hdr, sizeof(sec_hdr));
    sp_set_payload(&pkt, payload, sizeof(payload));

    uint8_t buf[256];
    size_t n = sp_packet_build_frame(&pkt, buf, sizeof(buf));
    if (n == 0)
        return 1;

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
        return 1;

    ASSERT_EQ_INT(parsed.sec_hdr_len, 5);
    ASSERT_EQ_MEM(parsed.sec_hdr, sec_hdr, sizeof(sec_hdr));
    ASSERT_EQ_INT(parsed.ph.version, 1);

    return 0;
}

static int test_bitfield_masking(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);

    const uint8_t payload[] = {0xFF};

    sp_set_primary_header(&pkt, 0xFF, 0xFF, 0, 0xFFFF, 3, 0xFFFF);
    sp_set_payload(&pkt, payload, sizeof(payload));

    uint8_t buf[256];
    size_t n = sp_packet_build_frame(&pkt, buf, sizeof(buf));
    if (n == 0)
        return 1;

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
        return 1;

    ASSERT_EQ_INT(parsed.ph.version, 7);
    ASSERT_EQ_INT(parsed.ph.type, 1);
    ASSERT_EQ_INT(parsed.ph.apid, 0x7FF);
    ASSERT_EQ_INT(parsed.ph.seq_count, 0x3FFF);

    return 0;
}

static int test_secheader_null(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);

    const uint8_t payload[] = {0x42};

    sp_set_primary_header(&pkt, 0, 0, 1, 0x100, SP_SEQ_FLAG_UNSEGMENTED, 1);
    sp_set_secondary_header(&pkt, NULL, 0);
    sp_set_payload(&pkt, payload, sizeof(payload));

    ASSERT_EQ_INT(pkt.ph.sec_hdr_flag, 0);

    uint8_t buf[256];
    size_t n = sp_packet_build_frame(&pkt, buf, sizeof(buf));
    if (n == 0)
        return 1;

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
        return 1;

    ASSERT_EQ_INT(parsed.ph.sec_hdr_flag, 0);
    ASSERT_EQ_INT(parsed.sec_hdr_len, 0);

    return 0;
}

static int test_parse_crc_total_too_small(void)
{
    uint8_t buf[8] = {0x08, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00};

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, sizeof(buf));
    return ok ? 1 : 0;
}

static int test_parse_no_crc_total_too_small(void)
{
    uint8_t buf[8] = {0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, sizeof(buf));
    return ok ? 1 : 0;
}

static int test_parse_no_crc_payload_truncated(void)
{
    uint8_t buf[8] = {0x08, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00};

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, sizeof(buf));
    return ok ? 1 : 0;
}

static int test_parse_declared_secheader_too_long(void)
{
    uint8_t buf[8] = {0x08, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x03};

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, sizeof(buf));
    return ok ? 1 : 0;
}

pus_test_result_t test_space_packet_run_all(void)
{
    RUN_TEST(test_roundtrip_no_secheader);
    RUN_TEST(test_roundtrip_with_secheader_crc);
    RUN_TEST(test_crc_mismatch);
    RUN_TEST(test_malformed_short_buffer);
    RUN_TEST(test_highlevel_api);
    RUN_TEST(test_null_pointers);
    RUN_TEST(test_secheader_no_crc);
    RUN_TEST(test_sequence_flags);
    RUN_TEST(test_version_and_type);
    RUN_TEST(test_buffer_too_small);
    RUN_TEST(test_empty_payload);
    RUN_TEST(test_crc_computation);
    RUN_TEST(test_parse_short_secheader);
    RUN_TEST(test_parse_short_payload_crc);
    RUN_TEST(test_secheader_extended);
    RUN_TEST(test_bitfield_masking);
    RUN_TEST(test_secheader_null);
    RUN_TEST(test_parse_crc_total_too_small);
    RUN_TEST(test_parse_no_crc_total_too_small);
    RUN_TEST(test_parse_no_crc_payload_truncated);
    RUN_TEST(test_parse_declared_secheader_too_long);

    pus_test_result_t r;
    r.total = cunit_total_tests;
    r.passed = cunit_total_tests - cunit_overall_failures;
    return r;
}
