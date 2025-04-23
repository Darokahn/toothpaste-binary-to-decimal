"""
Test whether a given bitwidth works with the algorithm
"""

import math

bitwidth = 8

decimalDigitWidth = math.ceil(math.log10(2**bitwidth - 1))

powers = [2**i for i in range(bitwidth)]

digits = [0] * decimalDigitWidth

for power in powers:
    string = str(power).zfill(decimalDigitWidth)
    for i in range(decimalDigitWidth-1, -1, -1):
        digits[i] += int(string[i])

print("digits:", digits)

for digit in digits:
    if not (digit <= 255):
        print("digit overflows before carry in worst case")

for i in range(decimalDigitWidth-1, 0, -1):
    digits[i-1] += digits[i] // 10
    digits[i] %= 10
    if not digits[i-1] <= 255:
        print(f"digit overflows during carry at position {i} in worst case")
