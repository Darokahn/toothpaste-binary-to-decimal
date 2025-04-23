#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/*
 * ROM int to string
 * (or "toothpaste itoa")
 * This algorithm uses the fullDecimal32_t union outlined below to create a ROM-based zero-padded itoa algorithm.
 * For each bit of the integer that is set to 1, this algorithm consults the ROM for the appropriate byte-per-decimal array corresponding to its place.
 * For the sixth bit, this is 32, {0, 0, 0, 0, 0, 0, 0, 0, 3, 2}. (note normalized to zero, not ascii).
 * The arith struct of the union is used to quickly accumulate the decimal array to the current buffer in 2 operations.
 * There's the rub: with 32 bit integers, in the worst case, no digit will ever overflow its 8-bit allotment.
 * Even when adding two decimals cast to numbers, the 8-bit allotments never bump elbows.
 * This is not true of 64-bit integers, which would need some workarounds to use this algorithm.
 * This allows us to lazily carry at the very end, "squeezing" the carry from right to left like a toothpaste tube.
 * For 64-bit integers, you could either carry at intervals rather than just once at the end, or you could instead use 16-bit slots for each allotment.
 * A good general interval to carry after is 25, as that is the very worst case with byte-per-decimal encoding when adding the largest decimal value every time.
 * Otherwise, you may find the optimal interval for your desired bitwidth, which is likely better than the worst case.
 * Finally, you may monitor your data to determine exactly when a carry is necessary.
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
    uint32_t number = 394789199;
    fullDecimal32_t decimal = uitodec(number);
    char str[11];
    fillBuffer(decimal, str);
    printf("%s\n", str);
    return 0;
}
