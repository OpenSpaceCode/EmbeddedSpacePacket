# EmbeddedSpacePacket
Minimum CCSDS SpacePacket implementation in C for embedded applications.

Contents
- tiny header and implementation in `include/` and `src/`
- example in `examples/`
- simple host test in `tests/`
- `Makefile` to build library, example and test

Build & run (host):

1. Build everything:

	make

2. Run example:

	./examples/spacepacket_example

3. Run tests:

	./tests/ctest

Notes
- This is a minimal, small-footprint implementation designed for embedded use.
- The primary header implemented is CCSDS-like (6 bytes). The packet length field
  in the header stores `payload_len - 1` when serializing and the parser reconstructs
  `payload_len = header_length + 1`.

CCSDS Space Packet (brief)
- **Primary header (6 bytes):** bytes 0-1 hold version (3 bits), packet type (1 bit),
	secondary header flag (1 bit) and APID (11 bits). Bytes 2-3 are the sequence control
	field: segmentation/sequence flags (2 bits) and packet sequence count (14 bits). Bytes
	4-5 are the Packet Length field (16 bits) which encodes the length of the Packet Data
	Field minus 1 (i.e., PacketLength = N-1 where N is data-field octets).
- **Packet Data Field:** follows the primary header and contains an optional secondary
	header (if the secondary header flag is set) followed by user/application data.
- **Secondary header:** optional, format and semantics are application-dependent (time
	tags, service headers, etc.). The presence is indicated by the secondary header flag.
- **Segmentation/Sequence Flags:** indicate if the packet is standalone (unsegmented),
	first, continuation or last segment when a logical packet is split over multiple
	sequence counts.
- **APID (Application Process ID):** identifies the source/consumer application or
	logical stream of packets.
- **Error control:** CCSDS Space Packet by itself does not mandate a particular CRC in
	the primary header; error control is usually provided in lower or higher layers or
	via application-specific fields. This repository adds an optional CRC covering the
	secondary header and payload as an example.


Links:
- https://ccsds.org/Pubs/133x0b2e2.pdf (CCSDS Space Packet Standard)   