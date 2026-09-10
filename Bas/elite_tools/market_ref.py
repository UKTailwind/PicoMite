"""Reference implementation of Elite's market.

Prices and availability are not stored either: each system draws one random
byte on arrival, and that byte plus the system's economy decides the price
and the quantity of all seventeen commodities. The arithmetic is 8-bit and
wraps, which is what makes narcotics and furs swing so wildly, so the wrap
is reproduced rather than tidied away.

  price    = ((base + (rnd AND mask) + economy * factor) AND 255) * 4   tenths of a credit
  quantity = base_qty + (rnd AND mask) - economy * factor
             clamped to 0 if that went negative, then AND 63

Economy runs 0 (Rich Industrial) to 7 (Poor Agricultural). A negative factor
means the goods are cheaper and more plentiful the more agricultural the
system is.

Usage:  python market_ref.py            Lave's market, the one everyone knows
        python market_ref.py checksum   one number over every economy and byte
"""
import sys

#        name            base factor unit qty  mask
ITEMS = [("Food",           19,  -2, "t",   6, 0x01),
         ("Textiles",       20,  -1, "t",  10, 0x03),
         ("Radioactives",   65,  -3, "t",   2, 0x07),
         ("Slaves",         40,  -5, "t", 226, 0x1F),
         ("Liquor/Wines",   83,  -5, "t", 251, 0x0F),
         ("Luxuries",      196,   8, "t",  54, 0x03),
         ("Narcotics",     235,  29, "t",   8, 0x78),
         ("Computers",     154,  14, "t",  56, 0x03),
         ("Machinery",     117,   6, "t",  40, 0x07),
         ("Alloys",         78,   1, "t",  17, 0x1F),
         ("Firearms",      124,  13, "t",  29, 0x07),
         ("Furs",          176,  -9, "t", 220, 0x3F),
         ("Minerals",       32,  -1, "t",  53, 0x03),
         ("Gold",           97,  -1, "kg", 66, 0x07),
         ("Platinum",      171,  -2, "kg", 55, 0x1F),
         ("Gem-Stones",     45,  -1, "g", 250, 0x0F),
         ("Alien items",    53,  15, "t", 192, 0x07)]


def market(economy, rnd):
    """Price in tenths of a credit and quantity, for all seventeen items."""
    out = []
    for name, base, factor, unit, qty, mask in ITEMS:
        price = ((base + (rnd & mask) + economy * factor) & 0xFF) * 4
        q = qty + (rnd & mask) - economy * factor
        if q < 0:
            q = 0
        q &= 63
        out.append((name, price, q, unit))
    return out


def checksum():
    h = 0
    for eco in range(8):
        for rnd in range(256):
            for _, price, q, _ in market(eco, rnd):
                h = (h * 31 + price) % 2147483647
                h = (h * 31 + q) % 2147483647
    return h


def main():
    if "checksum" in sys.argv:
        print("market checksum over 8 economies x 256 bytes x 17 items: %d" % checksum())
        return
    # Lave is economy 5, Rich Agricultural.  Its food is famously a few
    # credits and it sells no computers at all.
    print("Lave (economy 5), across every possible market byte:")
    print("%-14s %-14s %s" % ("item", "price (Cr)", "quantity"))
    for i, (name, base, factor, unit, qty, mask) in enumerate(ITEMS):
        lo = min(market(5, r)[i][1] for r in range(256))
        hi = max(market(5, r)[i][1] for r in range(256))
        qlo = min(market(5, r)[i][2] for r in range(256))
        qhi = max(market(5, r)[i][2] for r in range(256))
        print("%-14s %5.1f to %-6.1f %d to %d %s" % (name, lo / 10.0, hi / 10.0, qlo, qhi, unit))


if __name__ == "__main__":
    main()
