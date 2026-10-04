#include "cunit.h"
#include "space_packet.h"
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
    {
        return 1;
    }
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
    {
        return 1;
    }
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
    {
        return 1;
    }

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
    {
        return 1;
    }

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
    {
        return 1;
    }
    if (sp_packet_serialize(NULL, buf, sizeof(buf)) != 0)
    {
        return 1;
    }
    if (sp_packet_serialize(&pkt, NULL, sizeof(buf)) != 0)
    {
        return 1;
    }
    if (sp_packet_parse(NULL, buf, sizeof(buf)))
    {
        return 1;
    }
    if (sp_packet_parse(&pkt, NULL, sizeof(buf)))
    {
        return 1;
    }

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
        {
            return 1;
        }

        /* Verify wire bits directly: seq flags are bits 16-17, i.e. top 2 bits of buf[2]. */
        unsigned wire_flags = (unsigned)(buf[2] >> 6) & 0x3u;
        if (wire_flags != (unsigned)flags[i])
        {
            return 1;
        }

        sp_packet_t parsed;
        int ok = sp_packet_parse(&parsed, buf, n);
        if (!ok)
        {
            return 1;
        }
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
    {
        return 1;
    }

    /* Version is bits 0-2 of byte 0, i.e. the top 3 bits. */
    unsigned version_bits = (unsigned)(buf[0] >> 5) & 0x7u;
    ASSERT_EQ_INT(version_bits, 0);

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
    {
        return 1;
    }
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
    {
        return 1;
    }

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
    {
        return 1;
    }

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

static int test_zero_length_with_data_pointer(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);
    const uint8_t data[] = {0x01};
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TM, 0, 0x100, SP_SEQ_FLAG_UNSEGMENTED, 1);
    sp_set_data(&pkt, data, 0);

    uint8_t buf[32];
    ASSERT_EQ_INT(0, sp_packet_serialize_size(&pkt));
    ASSERT_EQ_INT(0, sp_packet_serialize(&pkt, buf, sizeof(buf)));
    return 0;
}

static int test_secondary_header_flag_on_wire(void)
{
    sp_packet_t pkt;
    sp_packet_init(&pkt);
    const uint8_t data[] = {0x01};
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TM, 1, 0x100, SP_SEQ_FLAG_UNSEGMENTED, 1);
    sp_set_data(&pkt, data, sizeof(data));

    uint8_t buf[32];
    size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
    ASSERT_TRUE(n != 0);

    /* Secondary Header Flag is bit 4 of the header, i.e. bit 3 of byte 0. */
    ASSERT_EQ_INT(1, (buf[0] >> 3) & 0x1u);

    sp_packet_t parsed;
    ASSERT_TRUE(sp_packet_parse(&parsed, buf, n));
    ASSERT_EQ_INT(1, parsed.ph.sec_hdr_flag);
    return 0;
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
    {
        return 1;
    }

    sp_packet_t parsed;
    int ok = sp_packet_parse(&parsed, buf, n);
    if (!ok)
    {
        return 1;
    }

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

static int test_parse_rejects_nonzero_version(void)
{
    /* Valid 7-byte packet except for the version bits (top 3 bits of byte 0). */
    for (unsigned version = 1; version <= 7u; version++)
    {
        uint8_t buf[7] = {0x08, 0x00, 0xC0, 0x00, 0x00, 0x00, 0xAA};
        buf[0] = (uint8_t)(buf[0] | (version << 5));
        sp_packet_t parsed;
        if (sp_packet_parse(&parsed, buf, sizeof(buf)))
        {
            return 1;
        }
    }
    return 0;
}

static int test_parse_rejects_max_length_field(void)
{
    /* length_field=0xFFFF → 65536 octets, beyond SP_PDF_MAX_LEN; reject even if the buffer is
     * large enough, and never accept it as a zero-length packet. */
    static uint8_t buf[SP_PRIMARY_HEADER_LEN + SP_PDF_MAX_LEN + 1U] =
        {0x08, 0x00, 0x00, 0x00, 0xFF, 0xFF};
    sp_packet_t parsed;
    if (sp_packet_parse(&parsed, buf, SP_PRIMARY_HEADER_LEN))
    {
        return 1;
    }
    return sp_packet_parse(&parsed, buf, sizeof(buf)) ? 1 : 0;
}

static int test_roundtrip_max_length(void)
{
    static uint8_t data[SP_PDF_MAX_LEN];
    static uint8_t buf[SP_PRIMARY_HEADER_LEN + SP_PDF_MAX_LEN];
    for (size_t i = 0; i < sizeof(data); i++)
    {
        data[i] = (uint8_t)(i * 31u);
    }

    sp_packet_t pkt;
    sp_packet_init(&pkt);
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TM, 0, 0x100, SP_SEQ_FLAG_UNSEGMENTED, 1);
    sp_set_data(&pkt, data, SP_PDF_MAX_LEN);

    size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
    ASSERT_TRUE(n == sizeof(buf));

    sp_packet_t parsed;
    ASSERT_TRUE(sp_packet_parse(&parsed, buf, n));
    ASSERT_EQ_INT(0xFFFE, parsed.ph.packet_length);
    ASSERT_EQ_INT(SP_PDF_MAX_LEN, parsed.data_len);
    ASSERT_EQ_MEM(parsed.data, data, sizeof(data));
    return 0;
}

static int test_parse_failure_leaves_out_untouched(void)
{
    /* Each buffer has header fields that differ from the sentinel, and fails a different check. */
    const uint8_t bad_version[] = {0x3F, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xAA};
    const uint8_t max_length[] = {0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xAA};
    const uint8_t truncated[] = {0x1F, 0xFF, 0xFF, 0xFF, 0x00, 0x05, 0xAA};
    const uint8_t *const cases[] = {bad_version, max_length, truncated};
    const uint8_t sentinel_data[] = {0x11, 0x22};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        sp_packet_t out;
        sp_packet_init(&out);
        sp_set_primary_header(&out, SP_PACKET_TYPE_TM, 0, 0x123, SP_SEQ_FLAG_FIRST_SEGMENT, 7);
        sp_set_data(&out, sentinel_data, sizeof(sentinel_data));
        out.ph.packet_length = 0x55;

        ASSERT_TRUE(!sp_packet_parse(&out, cases[i], sizeof(bad_version)));

        ASSERT_EQ_INT(0, out.ph.version);
        ASSERT_EQ_INT(SP_PACKET_TYPE_TM, out.ph.type);
        ASSERT_EQ_INT(0, out.ph.sec_hdr_flag);
        ASSERT_EQ_INT(0x123, out.ph.apid);
        ASSERT_EQ_INT(SP_SEQ_FLAG_FIRST_SEGMENT, out.ph.seq_flags);
        ASSERT_EQ_INT(7, out.ph.seq_count);
        ASSERT_EQ_INT(0x55, out.ph.packet_length);
        ASSERT_TRUE(out.data == sentinel_data);
        ASSERT_EQ_INT(sizeof(sentinel_data), out.data_len);
    }
    return 0;
}

/* --- Boundary tests ------------------------------------------------------- */

static int test_serialize_buffer_size_boundary(void)
{
    const uint8_t data[] = {1, 2, 3, 4};
    const size_t need = SP_PRIMARY_HEADER_LEN + sizeof(data);

    sp_packet_t pkt;
    sp_packet_init(&pkt);
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TM, 0, 0x100, SP_SEQ_FLAG_UNSEGMENTED, 1);
    sp_set_data(&pkt, data, sizeof(data));

    /* Heap buffers of the exact size, so a one-byte overrun is visible to ASan. */
    uint8_t *one_short = (uint8_t *)malloc(need - 1u);
    uint8_t *exact = (uint8_t *)malloc(need);
    int rc = 1;

    if (one_short && exact)
    {
        rc = !((sp_packet_serialize(&pkt, one_short, need - 1u) == 0) &&
               (sp_packet_serialize(&pkt, exact, need) == need) &&
               (memcmp(&exact[SP_PRIMARY_HEADER_LEN], data, sizeof(data)) == 0));
    }

    free(one_short);
    free(exact);
    return rc;
}

static int test_serialize_size_boundaries(void)
{
    static const uint8_t data[SP_PDF_MAX_LEN] = {0};
    sp_packet_t pkt;
    sp_packet_init(&pkt);

    sp_set_data(&pkt, data, 1);
    ASSERT_TRUE(sp_packet_serialize_size(&pkt) == (SP_PRIMARY_HEADER_LEN + 1u));

    sp_set_data(&pkt, data, SP_PDF_MAX_LEN);
    ASSERT_TRUE(sp_packet_serialize_size(&pkt) == (SP_PRIMARY_HEADER_LEN + SP_PDF_MAX_LEN));
    return 0;
}

static int test_roundtrip_min_length(void)
{
    const uint8_t data[] = {0x5A};
    sp_packet_t pkt;
    sp_packet_init(&pkt);
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TM, 0, 0x100, SP_SEQ_FLAG_UNSEGMENTED, 1);
    sp_set_data(&pkt, data, sizeof(data));

    uint8_t buf[SP_PRIMARY_HEADER_LEN + 1u];
    size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
    ASSERT_TRUE(n == sizeof(buf));

    /* One data octet → length count C = 0 (§4.1.3.5.3). */
    ASSERT_EQ_INT(0x00, buf[4]);
    ASSERT_EQ_INT(0x00, buf[5]);

    sp_packet_t parsed;
    ASSERT_TRUE(sp_packet_parse(&parsed, buf, n));
    ASSERT_EQ_INT(0, parsed.ph.packet_length);
    ASSERT_EQ_INT(1, parsed.data_len);
    ASSERT_EQ_INT(0x5A, parsed.data[0]);
    return 0;
}

static int test_parse_buffer_length_boundaries(void)
{
    /* Smallest valid packet: 6-byte header (length_field=0) + 1 data octet. */
    const uint8_t wire[] = {0x08, 0x00, 0xC0, 0x00, 0x00, 0x00, 0xAA};
    sp_packet_t parsed;

    /* Heap copies of the exact size, so a one-byte over-read is visible to ASan. */
    for (size_t len = 0; len <= sizeof(wire); len++)
    {
        uint8_t *buf = (uint8_t *)malloc(len ? len : 1u);
        if (!buf)
        {
            return 1;
        }
        memcpy(buf, wire, len);
        int ok = sp_packet_parse(&parsed, buf, len);
        int data_ok = ok && (parsed.data_len == 1) && (parsed.data[0] == 0xAA);
        free(buf);

        /* Header-only (6) and anything shorter must fail; only the full 7 bytes parse. */
        if (len < sizeof(wire))
        {
            ASSERT_TRUE(!ok);
        }
        else
        {
            ASSERT_TRUE(data_ok);
        }
    }
    return 0;
}

static int test_parse_ignores_trailing_bytes(void)
{
    /* length_field=1 → data_len=2; two extra bytes follow the packet. */
    const uint8_t buf[] = {0x08, 0x00, 0xC0, 0x00, 0x00, 0x01, 0x11, 0x22, 0x33, 0x44};
    sp_packet_t parsed;
    ASSERT_TRUE(sp_packet_parse(&parsed, buf, sizeof(buf)));
    ASSERT_EQ_INT(2, parsed.data_len);
    ASSERT_TRUE(parsed.data == &buf[SP_PRIMARY_HEADER_LEN]);
    return 0;
}

static int test_header_fields_all_zero(void)
{
    const uint8_t data[] = {0x00};
    sp_packet_t pkt;
    sp_packet_init(&pkt);
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TM, 0, 0x000, SP_SEQ_FLAG_CONTINUING_SEGMENT, 0);
    sp_set_data(&pkt, data, sizeof(data));

    uint8_t buf[SP_PRIMARY_HEADER_LEN + 1u];
    ASSERT_TRUE(sp_packet_serialize(&pkt, buf, sizeof(buf)) == sizeof(buf));

    const uint8_t expected[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ASSERT_EQ_MEM(buf, expected, sizeof(expected));

    sp_packet_t parsed;
    ASSERT_TRUE(sp_packet_parse(&parsed, buf, sizeof(buf)));
    ASSERT_EQ_INT(SP_PACKET_TYPE_TM, parsed.ph.type);
    ASSERT_EQ_INT(0, parsed.ph.sec_hdr_flag);
    ASSERT_EQ_INT(0, parsed.ph.apid);
    ASSERT_EQ_INT(SP_SEQ_FLAG_CONTINUING_SEGMENT, parsed.ph.seq_flags);
    ASSERT_EQ_INT(0, parsed.ph.seq_count);
    return 0;
}

static int test_header_fields_all_max(void)
{
    const uint8_t data[] = {0x00};
    sp_packet_t pkt;
    sp_packet_init(&pkt);
    sp_set_primary_header(&pkt, SP_PACKET_TYPE_TC, 1, 0x7FF, SP_SEQ_FLAG_UNSEGMENTED, 0x3FFF);
    sp_set_data(&pkt, data, sizeof(data));

    uint8_t buf[SP_PRIMARY_HEADER_LEN + 1u];
    ASSERT_TRUE(sp_packet_serialize(&pkt, buf, sizeof(buf)) == sizeof(buf));

    /* Version stays 000, so the first octet is 0x1F, not 0xFF. */
    const uint8_t expected[] = {0x1F, 0xFF, 0xFF, 0xFF, 0x00, 0x00};
    ASSERT_EQ_MEM(buf, expected, sizeof(expected));

    sp_packet_t parsed;
    ASSERT_TRUE(sp_packet_parse(&parsed, buf, sizeof(buf)));
    ASSERT_EQ_INT(SP_PACKET_TYPE_TC, parsed.ph.type);
    ASSERT_EQ_INT(1, parsed.ph.sec_hdr_flag);
    ASSERT_EQ_INT(0x7FF, parsed.ph.apid);
    ASSERT_EQ_INT(SP_SEQ_FLAG_UNSEGMENTED, parsed.ph.seq_flags);
    ASSERT_EQ_INT(0x3FFF, parsed.ph.seq_count);
    return 0;
}

test_result_t test_space_packet_run_all(void)
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
    RUN_TEST(test_zero_length_with_data_pointer);
    RUN_TEST(test_secondary_header_flag_on_wire);
    RUN_TEST(test_bitfield_masking);
    RUN_TEST(test_parse_data_just_short);
    RUN_TEST(test_parse_data_truncated);
    RUN_TEST(test_parse_data_far_too_short);
    RUN_TEST(test_parse_rejects_nonzero_version);
    RUN_TEST(test_parse_rejects_max_length_field);
    RUN_TEST(test_roundtrip_max_length);
    RUN_TEST(test_parse_failure_leaves_out_untouched);
    RUN_TEST(test_serialize_buffer_size_boundary);
    RUN_TEST(test_serialize_size_boundaries);
    RUN_TEST(test_roundtrip_min_length);
    RUN_TEST(test_parse_buffer_length_boundaries);
    RUN_TEST(test_parse_ignores_trailing_bytes);
    RUN_TEST(test_header_fields_all_zero);
    RUN_TEST(test_header_fields_all_max);

    test_result_t r;
    r.total = cunit_total_tests;
    r.passed = cunit_total_tests - cunit_overall_failures;
    return r;
}
