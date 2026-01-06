# EmbeddedSpacePacket
Minimum SpacePacket implementation in C for embedded applications

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

	./tests/test_packet

Notes
- This is a minimal, small-footprint implementation designed for embedded use.
- The primary header implemented is CCSDS-like (6 bytes). The packet length field
  in the header stores `payload_len - 1` when serializing and the parser reconstructs
  `payload_len = header_length + 1`.

