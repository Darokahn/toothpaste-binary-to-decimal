#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/*
 * ROM-based integer to string conversion
 * (a.k.a. "toothpaste itoa")
 *
 * This algorithm converts a 32-bit integer to a zero-padded decimal string using a ROM lookup method.
 * It relies on the `fullDecimal32_t` union described below, which provides an accumulator (as a struct)
 * for adding decimal digits efficiently.
 *
 * For each bit set to 1 in the input integer, the corresponding precomputed decimal representation
 * (a byte-per-digit array) is fetched from ROM and added to the accumulator.
 * For example, bit 5 (value 32) corresponds to: {0, 0, 0, 0, 0, 0, 0, 0, 3, 2} — normalized to 0–9 digits (not ASCII).
 *
 * The addition is optimized: each decimal array is treated as two integers, allowing the full array
 * to be added in just two steps.
 *
 * Crucially, in 32-bit integers, each decimal digit stays within its 8-bit slot during addition —
 * no intermediate carries are needed. This makes it safe to delay carry propagation until the end.
 * The final carry is applied from right to left — like squeezing a tube of toothpaste.
 * Carry is optimized to use no division by employing quotient and remainder ROMs. This is relatively cheap because the dividend can only be one of 256 values.
 *
 * This doesn’t hold for 64-bit integers — overflow may occur during intermediate steps.
 * To adapt this algorithm for 64-bit integers, consider:
 *   - Applying carry propagation periodically (e.g., every 25 adds — worst case if all addends were 999 ... 999),
 *   - Using 16-bit slots per digit to make overflow a non-concern, or
 *   - Monitoring the accumulator to perform carries only when needed.
 *
 * This technique allows fast, carry-lazy integer-to-decimal conversion using simple byte math.
 */

uint32_t leftmostBit = 0x80000000;

// for reference: A decimal is "happy" when none of its bytes has value > 9

uint8_t quotients[] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25
};
uint8_t remainders[] = {
0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5
};

typedef union {
    uint8_t digits[10];
    struct {
        uint64_t high;
        uint16_t low;
    } arith;
} fullDecimal32_t; // 10 bytes to encode at most the largest 32-bit number in decimal with 1 byte each.

fullDecimal32_t addDecimalLazy(fullDecimal32_t d1, fullDecimal32_t d2) {
    // Adding unhappy decimals involves a risk of overflow. Assuming you start with happy decimals, the risk is higher the more decimals you add as well as the closer to 9 each digit is.
    // This can be useful if you are dealing with a set of integers with a known worst case, and that worst case does not cause overflow. Lazily carrying after all operations will be more efficient.
    // Otherwise, use `addDecimal`
    fullDecimal32_t result;
    result.arith.high = d1.arith.high + d2.arith.high;
    result.arith.low = d1.arith.low + d2.arith.low;
    return result;
}

fullDecimal32_t carryDecimal(fullDecimal32_t decimal) {
    for (int i = 9; i > 0; i--) {
        decimal.digits[i-1] += quotients[decimal.digits[i]];
        decimal.digits[i] = remainders[decimal.digits[i]];
    }
    return decimal;
}

fullDecimal32_t addDecimal(fullDecimal32_t d1, fullDecimal32_t d2) {
    // Use this function on two happy decimals to ensure no overflow
    fullDecimal32_t result = addDecimalLazy(d1, d2);
    result = carryDecimal(result);
    return result;
}

int fillBuffer(fullDecimal32_t decimal, char buffer[11]) {
    int decimalPtr = 0;
    int bufferPtr;

    if (decimal.arith.low == 0 && decimal.arith.high == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }

    while (decimal.digits[decimalPtr] == 0) decimalPtr++;
    bufferPtr = 0;

    while (decimalPtr < 10) {
        buffer[bufferPtr] = decimal.digits[decimalPtr] + '0';
        bufferPtr++;
        decimalPtr++;
    }
    buffer[bufferPtr] = '\0';
    return bufferPtr;
}

const fullDecimal32_t decimalROM[] = {
    (fullDecimal32_t){ .digits = {2, 1, 4, 7, 4, 8, 3, 6, 4, 8} }, // 2^31 = 2147483648
    (fullDecimal32_t){ .digits = {1, 0, 7, 3, 7, 4, 1, 8, 2, 4} }, // 2^30 = 1073741824
    (fullDecimal32_t){ .digits = {0, 5, 3, 6, 8, 7, 0, 9, 1, 2} }, // 2^29 = 536870912
    (fullDecimal32_t){ .digits = {0, 2, 6, 8, 4, 3, 5, 4, 5, 6} }, // 2^28 = 268435456
    (fullDecimal32_t){ .digits = {0, 1, 3, 4, 2, 1, 7, 7, 2, 8} }, // 2^27 = 134217728
    (fullDecimal32_t){ .digits = {0, 0, 6, 7, 1, 0, 8, 8, 6, 4} }, // 2^26 =  67108864
    (fullDecimal32_t){ .digits = {0, 0, 3, 3, 5, 5, 4, 4, 3, 2} }, // 2^25 =  33554432
    (fullDecimal32_t){ .digits = {0, 0, 1, 6, 7, 7, 7, 2, 1, 6} }, // 2^24 =  16777216
    (fullDecimal32_t){ .digits = {0, 0, 0, 8, 3, 8, 8, 6, 0, 8} }, // 2^23 =   8388608
    (fullDecimal32_t){ .digits = {0, 0, 0, 4, 1, 9, 4, 3, 0, 4} }, // 2^22 =   4194304
    (fullDecimal32_t){ .digits = {0, 0, 0, 2, 0, 9, 7, 1, 5, 2} }, // 2^21 =   2097152
    (fullDecimal32_t){ .digits = {0, 0, 0, 1, 0, 4, 8, 5, 7, 6} }, // 2^20 =   1048576
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 5, 2, 4, 2, 8, 8} }, // 2^19 =    524288
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 2, 6, 2, 1, 4, 4} }, // 2^18 =    262144
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 1, 3, 1, 0, 7, 2} }, // 2^17 =    131072
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 6, 5, 5, 3, 6} }, // 2^16 =     65536
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 3, 2, 7, 6, 8} }, // 2^15 =     32768
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 1, 6, 3, 8, 4} }, // 2^14 =     16384
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 8, 1, 9, 2} }, // 2^13 =      8192
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 4, 0, 9, 6} }, // 2^12 =      4096
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 2, 0, 4, 8} }, // 2^11 =      2048
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 1, 0, 2, 4} }, // 2^10 =      1024
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 0, 5, 1, 2} }, // 2^9  =       512
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 0, 2, 5, 6} }, // 2^8  =       256
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 0, 1, 2, 8} }, // 2^7  =       128
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 0, 0, 6, 4} }, // 2^6  =        64
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 0, 0, 3, 2} }, // 2^5  =        32
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 0, 0, 1, 6} }, // 2^4  =        16
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 0, 0, 0, 8} }, // 2^3  =         8
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 0, 0, 0, 4} }, // 2^2  =         4
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 0, 0, 0, 2} }, // 2^1  =         2
    (fullDecimal32_t){ .digits = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1} }  // 2^0  =         1
};

fullDecimal32_t uitodec(uint32_t i) {
    fullDecimal32_t a;
    a.arith.high = 0;
    a.arith.low = 0;
    int8_t count = 0;
    while (i) {
        while (!(i & leftmostBit)) {
            count++;
            i <<= 1;
        }
        a = addDecimalLazy(a, decimalROM[count]);
        count += 1;
        i <<= 1;
    }
    // squeeze the accumulated carries from right to left, like a toothpaste tube.
    a = carryDecimal(a);
    return a;
}

int main() {
    uint32_t number = 0;
    fullDecimal32_t decimal = uitodec(number);
    char str[11];
    fillBuffer(decimal, str);
    printf("%s\n", str);
    return 0;
}
