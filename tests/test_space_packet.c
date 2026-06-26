#include "../include/space_packet.h"
#include "cunit.h"
#include "test_runners.h"

#include <stdlib.h>
#include <string.h>

static int test_roundtrip_basic(void)
{
    const uint8_t data[] = {1, 2, 3, 4, 5};
    sp_packet_t pkt = {0};
    pkt.ph.apid = 0x01;
    pkt.ph.seq_count = 0x2;
    sp_set_data(&pkt, data, sizeof(data));

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
    ASSERT_EQ_INT(parsed.data_len, (int)sizeof(data));
    ASSERT_EQ_MEM(parsed.data, data, sizeof(data));

    free(buf);
    return 0;
}

static int test_roundtrip_with_secheader_flag(void)
{
    /* The Secondary Header Flag in the primary header signals that the first
     * bytes of the Packet Data Field are a secondary header. Layout and content
     * are mission-specific; here we use 2 header bytes followed by payload. */
    const uint8_t data[] = {0x00, 0x00, 10, 11, 12, 13};

    sp_packet_t pkt = {0};
    pkt.ph.apid = 0x123;
    pkt.ph.sec_hdr_flag = 1;
    pkt.ph.seq_flags = SP_SEQ_FLAG_UNSEGMENTED;
    pkt.ph.seq_count = 0x3;
    sp_set_data(&pkt, data, sizeof(data));

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
    ASSERT_EQ_INT(parsed.ph.sec_hdr_flag, 1);
    ASSERT_EQ_INT(parsed.ph.seq_flags, SP_SEQ_FLAG_UNSEGMENTED);
    ASSERT_EQ_INT(parsed.data_len, (int)sizeof(data));
    ASSERT_EQ_MEM(parsed.data, data, sizeof(data));

    free(buf);
    return 0;
}

static int test_malformed_short_buffer(void)
{
    uint8_t tiny[] = {0x00, 0x01};
    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, tiny, sizeof(tiny));
    return ok ? 1 : 0; /* expect failure */
}

static int test_highlevel_api(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);

    const uint8_t data[] = {'T', 'E', 'S', 'T'};
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TM, 0, 0x456, SP_SEQ_FLAG_FIRST_SEGMENT, 42);
    sp_set_data(&pkt, data, sizeof(data));

    uint8_t buf[256];
    size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
    if (n == 0)
        return 1;

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
        return 1;

    ASSERT_EQ_INT(parsed.ph.apid, 0x456);
    ASSERT_EQ_INT(parsed.ph.seq_count, 42);
    ASSERT_EQ_INT(parsed.ph.seq_flags, SP_SEQ_FLAG_FIRST_SEGMENT);
    ASSERT_EQ_INT(parsed.data_len, (int)sizeof(data));
    ASSERT_EQ_MEM(parsed.data, data, sizeof(data));

    return 0;
}

static int test_null_pointers(void)
{
    sp_packet_t pkt = {0};
    uint8_t buf[64];

    if (sp_packet_serialize_size(NULL) != 0)
        return 1;
    if (sp_packet_serialize(NULL, buf, sizeof(buf)) != 0)
        return 1;
    if (sp_packet_serialize(&pkt, NULL, sizeof(buf)) != 0)
        return 1;
    if (sp_packet_parse(NULL, buf, sizeof(buf)))
        return 1;
    if (sp_packet_parse(&pkt, NULL, sizeof(buf)))
        return 1;

    sp_packet_init(NULL);
    sp_set_primary_header(NULL, SP_PACKET_TYPE_TM, 0, 0, SP_SEQ_FLAG_UNSEGMENTED, 0);
    sp_set_data(NULL, buf, 10);

    return 0;
}

static int test_sequence_flags(void)
{
    const sp_seq_flag_t flags[] = {SP_SEQ_FLAG_UNSEGMENTED,
                                   SP_SEQ_FLAG_FIRST_SEGMENT,
                                   SP_SEQ_FLAG_CONTINUING_SEGMENT,
                                   SP_SEQ_FLAG_LAST_SEGMENT};
    const uint8_t data[] = {0x42};

    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
    {
        sp_packet_t pkt;
        sp_packet_init(&pkt);
        sp_set_primary_header(&pkt, SP_PACKET_TYPE_TM, 0, 0x100, flags[i], (uint16_t)i);
        sp_set_data(&pkt, data, sizeof(data));

        uint8_t buf[256];
        size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
        if (n == 0)
            return 1;

        /* Verify wire bits directly: seq flags are bits 16-17, i.e. top 2 bits of buf[2]. */
        unsigned wire_flags = (unsigned)(buf[2] >> 6) & 0x3u;
        if (wire_flags != (unsigned)flags[i])
            return 1;

        sp_packet_t parsed;
        int ok = sp_packet_parse(&parsed, buf, n);
        if (!ok)
            return 1;
        ASSERT_EQ_INT(parsed.ph.seq_flags, flags[i]);
    }

    return 0;
}

static int test_version_is_zero(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);
    const uint8_t data[] = {0x01};
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TM, 0, 0x100, SP_SEQ_FLAG_UNSEGMENTED, 0);
    sp_set_data(&pkt, data, sizeof(data));

    uint8_t buf[32];
    size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
    if (n == 0)
        return 1;

    /* Version is bits 0-2 of byte 0, i.e. the top 3 bits. */
    unsigned version_bits = (unsigned)(buf[0] >> 5) & 0x7u;
    ASSERT_EQ_INT(version_bits, 0);

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
        return 1;
    ASSERT_EQ_INT(parsed.ph.version, 0);

    return 0;
}

static int test_type_and_fields(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);
    const uint8_t data[] = {0x99};
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TC, 0, 0x7FF, SP_SEQ_FLAG_UNSEGMENTED, 0x3FFF);
    sp_set_data(&pkt, data, sizeof(data));

    uint8_t buf[256];
    size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
    if (n == 0)
        return 1;

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
        return 1;

    ASSERT_EQ_INT(parsed.ph.version, 0);
    ASSERT_EQ_INT(parsed.ph.type, SP_PACKET_TYPE_TC);
    ASSERT_EQ_INT(parsed.ph.apid, 0x7FF);
    ASSERT_EQ_INT(parsed.ph.seq_count, 0x3FFF);

    return 0;
}

static int test_buffer_too_small(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);

    const uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TM, 0, 0x100, SP_SEQ_FLAG_UNSEGMENTED, 1);
    sp_set_data(&pkt, data, sizeof(data));

    uint8_t buf[8]; /* too small: need 6 + 10 = 16 bytes */
    size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
    return (n == 0) ? 0 : 1;
}

static int test_empty_data(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TM, 0, 0x100, SP_SEQ_FLAG_UNSEGMENTED, 1);
    sp_set_data(&pkt, NULL, 0);

    uint8_t buf[256];
    size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
    return (n == 0) ? 0 : 1; /* serializer must reject empty Packet Data Field */
}

static int test_bitfield_masking(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);
    const uint8_t data[] = {0xFF};

    /* Pass oversized values; expect masking to valid widths. */
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TC, 0, 0xFFFF, 3, 0xFFFF);
    sp_set_data(&pkt, data, sizeof(data));

    uint8_t buf[256];
    size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
    if (n == 0)
        return 1;

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
        return 1;

    ASSERT_EQ_INT(parsed.ph.version, 0);              /* always 0, not masked from input */
    ASSERT_EQ_INT(parsed.ph.type, SP_PACKET_TYPE_TC); /* TC = 1 */
    ASSERT_EQ_INT(parsed.ph.apid, 0x7FF);             /* 0xFFFF & 0x7FF */
    ASSERT_EQ_INT(parsed.ph.seq_count, 0x3FFF);       /* 0xFFFF & 0x3FFF */

    return 0;
}

/* --- Parse rejection tests ------------------------------------------------ */

static int test_parse_data_just_short(void)
{
    /* length_field=1 → data_len=2, but only 1 byte follows the 6-byte header */
    uint8_t buf[7] = {0x08, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};
    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, sizeof(buf));
    return ok ? 1 : 0;
}

static int test_parse_data_truncated(void)
{
    /* length_field=2 → data_len=3, but only 2 bytes follow the header */
    uint8_t buf[8] = {0x08, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00};
    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, sizeof(buf));
    return ok ? 1 : 0;
}

static int test_parse_data_far_too_short(void)
{
    /* length_field=5 → data_len=6, but only 2 bytes follow the header */
    uint8_t buf[8] = {0x08, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00};
    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, sizeof(buf));
    return ok ? 1 : 0;
}

pus_test_result_t test_space_packet_run_all(void)
{
    RUN_TEST(test_roundtrip_basic);
    RUN_TEST(test_roundtrip_with_secheader_flag);
    RUN_TEST(test_malformed_short_buffer);
    RUN_TEST(test_highlevel_api);
    RUN_TEST(test_null_pointers);
    RUN_TEST(test_sequence_flags);
    RUN_TEST(test_version_is_zero);
    RUN_TEST(test_type_and_fields);
    RUN_TEST(test_buffer_too_small);
    RUN_TEST(test_empty_data);
    RUN_TEST(test_bitfield_masking);
    RUN_TEST(test_parse_data_just_short);
    RUN_TEST(test_parse_data_truncated);
    RUN_TEST(test_parse_data_far_too_short);

    pus_test_result_t r;
    r.total = cunit_total_tests;
    r.passed = cunit_total_tests - cunit_overall_failures;
    return r;
}
