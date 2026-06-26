# EmbeddedSpacePacket

Minimal, embedded-optimized implementation of the **CCSDS Space Packet Protocol**
primary header in C99, targeting small-scale academic missions.

## Standards Alignment

- **CCSDS 133.0-B-2** — Space Packet Protocol (Blue Book, June 2020)
- **CCSDS 130.3-G-1** — Space Packet Protocols Informational Report (Green Book, April 2023)

The library implements the 6-byte Primary Header exactly as specified. The Packet
Data Field (everything that follows the primary header) is treated as an opaque,
application-owned byte sequence. Its internal layout — secondary header, user data,
any integrity check — is mission-specific and outside the library's scope, consistent
with CCSDS §4.1.4.2 ("contents of the Packet Secondary Header shall be specified by
the source end user").

## Features

- **Primary Header serializer / parser** — big-endian, bit-exact per CCSDS §4.1.3
- **Sequence Flags** — wire values match the standard (`UNSEGMENTED = 0b11 = 3`)
- **Version enforcement** — always serializes Packet Version Number as `000` (§4.1.3.2)
- **Zero allocation** — no dynamic memory inside the library; caller supplies all buffers
- **Minimal footprint** — single header + single source file, no external dependencies
- **Pure C99** — no OS primitives, suitable for bare-metal targets

## Protocol Notes

**Primary header (6 bytes):**

| Bits  | Field                 | Notes                                                   |
| ----- | --------------------- | ------------------------------------------------------- |
| 0–2   | Packet Version Number | Always `000` (§4.1.3.2)                                 |
| 3     | Packet Type           | `0` = TM (telemetry), `1` = TC (telecommand)            |
| 4     | Secondary Header Flag | `1` if Packet Data Field starts with a secondary header |
| 5–15  | APID                  | 11-bit application process identifier                   |
| 16–17 | Sequence Flags        | See table below                                         |
| 18–31 | Packet Sequence Count | Modulo-16384 per APID (§4.1.3.4.3)                      |
| 32–47 | Packet Data Length    | (Packet Data Field octets) − 1 (§4.1.3.5)               |

**Sequence Flags wire values (§4.1.3.4.2):**

| Value    | Meaning                         |
| -------- | ------------------------------- |
| `00` = 0 | Continuation segment            |
| `01` = 1 | First segment                   |
| `10` = 2 | Last segment                    |
| `11` = 3 | Unsegmented (standalone packet) |

**Packet Data Field:**
Follows the primary header immediately. Contains an optional secondary header and
user data (§4.1.4). Structure is entirely mission-defined. The library copies it
verbatim on serialization and exposes a pointer into the wire buffer on parsing.

**CRC / integrity:**
CCSDS SPP defines no checksum at the Space Packet level. If a mission requires data
integrity checking (e.g. CRC-16-CCITT), it should be computed by the application and
appended to the Packet Data Field before calling `sp_packet_serialize`. See
`examples/main.c` for a complete example.

## Project Structure

```
EmbeddedSpacePacket/
├── include/
│   └── space_packet.h       # Public API and types
├── src/
│   └── space_packet.c       # Serializer, parser, helpers
├── examples/
│   └── main.c               # Example: build and parse a packet with app-level CRC
├── tests/
│   ├── cunit.h              # Minimal unit test framework
│   ├── test_runners.h       # Suite runner declarations
│   ├── test_space_packet.c  # Space Packet test suite
│   └── unit_tests.c         # Test entry point
├── tools/
│   └── coverage_html.sh     # gcovr HTML coverage report
├── build/                   # Build artifacts (git-ignored)
├── docs/                    # CCSDS reference PDFs
├── Makefile
└── README.md
```

## Building

```bash
make          # library + example + tests
make lib      # static library only  → build/libspacepacket.a
make example  # example binary       → build/examples/spacepacket_example
make test     # build and run tests
```

### Coverage (requires `gcovr`)

```bash
sudo apt install gcovr
make coverage-html   # → build/coverage/index.html
```

### Clean

```bash
make clean
```

## Quick Start

### Create and serialize a Space Packet

```c
#include "space_packet.h"

int main(void)
{
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};

    sp_packet_t pkt;
    sp_packet_init(&pkt);
    sp_set_primary_header(&pkt,
                          SP_PACKET_TYPE_TM,
                          0,     /* no secondary header */
                          0x100, /* APID */
                          SP_SEQ_FLAG_UNSEGMENTED,
                          1 /* sequence count */);
    sp_set_data(&pkt, data, sizeof(data));

    uint8_t buf[256];
    size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
    /* buf[0..n) holds the complete Space Packet */
}
```

### Parse an incoming packet

```c
sp_packet_t parsed;
if (sp_packet_parse(&parsed, buf, buf_len))
{
    /* parsed.data points into buf — no copy made */
    printf("APID=0x%03X seq=%u data=%u bytes\n",
           parsed.ph.apid,
           parsed.ph.seq_count,
           parsed.data_len);
}
else
{
    /* short buffer or buf_len < 6 + declared data length */
}
```

### Application-level CRC (see examples/main.c for full code)

```c
/* Append CRC to Packet Data Field before serializing: */
uint16_t crc = crc16_ccitt(pkt_data, payload_bytes);
pkt_data[off++] = (uint8_t)(crc >> 8);
pkt_data[off++] = (uint8_t)(crc & 0xFF);
sp_set_data(&pkt, pkt_data, (uint16_t)off);

/* Verify after parsing: */
size_t crc_area = parsed.data_len - 2;
uint16_t recv = ((uint16_t)parsed.data[crc_area] << 8) | parsed.data[crc_area + 1];
uint16_t calc = crc16_ccitt(parsed.data, crc_area);
if (recv != calc)
{ 
    /* integrity failure */
}
```

### Lifecycle

```c
void sp_packet_init(sp_packet_t *pkt);
```

### Building a packet

```c
/* Set primary header. Version is always forced to 000. */
void sp_set_primary_header(sp_packet_t *pkt,
                           uint8_t type,
                           uint8_t sec_hdr_flag,
                           uint16_t apid,
                           sp_seq_flag_t seq_flags,
                           uint16_t seq_count);

/* Bind the Packet Data Field (not copied; caller keeps memory alive). */
void sp_set_data(sp_packet_t *pkt, const uint8_t *data, uint16_t data_len);
```

### Serialization and parsing

```c
/* Required buffer size, or 0 if pkt is NULL or data_len == 0. */
size_t sp_packet_serialize_size(const sp_packet_t *pkt);

/* Write packet to buf. Returns bytes written, or 0 on error.
 * Fails if data is NULL, data_len == 0, or buf is too small. */
size_t sp_packet_serialize(const sp_packet_t *pkt, uint8_t *buf, size_t buf_len);

/* Parse wire bytes into *out. out->data points into buf (zero-copy).
 * Returns 1 on success, 0 on failure. */
int sp_packet_parse(sp_packet_t *out, const uint8_t *buf, size_t buf_len);
```

## Memory Footprint (estimated, 64-bit host)

| Item                    | Size                 |
| ----------------------- | -------------------- |
| `sp_packet_t` struct    | ~28 bytes            |
| Library code (stripped) | < 1 KB               |
| Serialization buffer    | 6 + `data_len` bytes |
| Heap usage              | none                 |

## Limitations

- No fragmentation / reassembly — sequence flag tracking is the application's responsibility
- No thread safety — external locking required in multi-threaded contexts
- Parser does not copy the Packet Data Field; caller must keep the source buffer alive

## References

- [CCSDS 133.0-B-2: Space Packet Protocol](https://public.ccsds.org/Pubs/133x0b2e2.pdf)
- [CCSDS 130.3-G-1: Space Packet Protocols (Green Book)](https://public.ccsds.org/Pubs/130x3g1.pdf)

## License

See LICENSE file.
