import struct, zlib, sys
MAGIC = 0x424F4F54
def main():
    if len(sys.argv) != 3:
        print("usage: pack_ota.py <app_raw.bin> <app_ota_full.bin>")
        sys.exit(1)
    with open(sys.argv[1], "rb") as f:
        pl = f.read()
    crc = zlib.crc32(pl) & 0xFFFFFFFF
    hdr = struct.pack("<III", MAGIC, len(pl), crc)
    with open(sys.argv[2], "wb") as f:
        f.write(hdr + pl)
    print(len(pl), hex(crc))
main()