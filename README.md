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
- **Version enforcement** — always serializes Packet Version Number as `000` and rejects any other value on parse (§4.1.3.2)
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

### Sanitizers (ASan + UBSan)

```bash
make sanitize        # rebuild with ASan + UBSan, run tests and example, then clean up
```

### Clean

```bash
make clean
```

## Quick Start

Minimal encode / decode round trip, condensed from [examples/main.c](examples/main.c):

```c
#include "space_packet.h"

const uint8_t payload[] = {'H', 'e', 'l', 'l', 'o', ' ', 'S', 'P'};

/* Encode */
sp_packet_t pkt;
sp_packet_init(&pkt);
sp_set_primary_header(&pkt,
                      SP_PACKET_TYPE_TM,
                      0,     /* no secondary header */
                      0x100, /* APID */
                      SP_SEQ_FLAG_UNSEGMENTED,
                      1 /* sequence count */);
sp_set_data(&pkt, payload, sizeof(payload));

uint8_t buf[256];
size_t n = sp_packet_serialize(&pkt, buf, sizeof(buf));
if (n == 0)
{
    /* NULL args, empty data, or buffer too small */
}

/* Decode */
sp_packet_t parsed;
if (!sp_packet_parse(&parsed, buf, n))
{
    /* buffer shorter than declared data length */
}
/* parsed.ph.apid, parsed.ph.seq_count, parsed.data, parsed.data_len
 * parsed.data points into buf (zero-copy) — keep buf alive. */
```

The full example in [examples/main.c](examples/main.c) additionally shows a
mission-defined secondary header and an application-level CRC-16-CCITT
(`make example`).

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
