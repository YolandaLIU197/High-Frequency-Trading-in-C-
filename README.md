# HW 1 — Order-book metrics in C++

## Overview

This program analyzes order-book snapshots represented by arrays of bid and ask prices and quantities. For each snapshot, it identifies the best bid and best ask and calculates the mid-price, spread, microprice, and obi.

## Compilation and Execution

Compile the program using C++17:

```bash
g++ -std=c++17 -Wall -Wextra hw1.cpp -o hw1
```

Run the program:

```bash
./hw1
```

## Metrics

The program calculates the following metrics using the best bid and best ask:

* Mid-price: `(best bid + best ask) / 2`
* Spread: `best ask - best bid`
* Microprice: `(best ask × bid size + best bid × ask size) / (bid size + ask size)`
* OBI: `(bid size - ask size) / (bid size + ask size)`

## Sanity Checks

The calculations were tested using three cases. All three cases use a bid price of 100.00 and an ask price of 100.02, allowing the effect of quantity changes to be examined while prices remain constant.

| Case      | Bid Size | Ask Size |      Mid | Spread | Microprice |     OBI |
| --------- | -------: | -------: | -------: | -----: | ---------: | ------: |
| Balanced  |      500 |      500 | 100.0100 | 0.0200 |   100.0100 |  0.0000 |
| Bid-heavy |      900 |      100 | 100.0100 | 0.0200 |   100.0180 |  0.8000 |
| Ask-heavy |      100 |      900 | 100.0100 | 0.0200 |   100.0020 | -0.8000 |

In the balanced case, the microprice equals the mid-price and OBI equals zero. In the bid-heavy case, the microprice moves toward the ask price. In the ask-heavy case, it moves toward the bid price.

## Locked and Crossed Books

The program calculates the spread as `best ask - best bid`.

A locked book, where the best ask equals the best bid, produces a spread of zero. A crossed book, where the best ask is below the best bid, produces a negative spread. The program preserves the calculated value and prints a warning rather than replacing it with another value.

## Snapshot Sequence and Results

I constructed five snapshots with three bid levels and three ask levels per snapshot. The arrays are intentionally unsorted, so the program searches all levels to identify the highest bid and lowest ask. It then calculates all four metrics using the prices and quantities at those best levels.

| Snapshot | Best Bid | Bid Size | Best Ask | Ask Size | Mid | Spread | Microprice | OBI |
| --------: | -------: | -------: | -------: | -------: | --------: | -------: | ---------: | -------: |
| 1 | 99.99 | 200 | 100.01 | 800 | 100.0000 | 0.0200 | 99.9940 | -0.6000 |
| 2 | 99.99 | 350 | 100.01 | 650 | 100.0000 | 0.0200 | 99.9970 | -0.3000 |
| 3 | 99.99 | 500 | 100.01 | 500 | 100.0000 | 0.0200 | 100.0000 | 0.0000 |
| 4 | 99.99 | 700 | 100.01 | 300 | 100.0000 | 0.0200 | 100.0040 | 0.4000 |
| 5 | 100.01 | 800 | 100.03 | 200 | 100.0200 | 0.0200 | 100.0260 | 0.6000 |

The sequence is constructed so that displayed liquidity gradually shifts from the ask side to the bid side. Consequently, OBI increases from -0.60 to 0.60. The mid-price remains at 100.00 for the first four snapshots and increases to 100.02 in the fifth snapshot.

A rising OBI here suggests increasing buying pressure because more quantity is resting at the best bid relative to the best ask. It could indicate that displayed liquidity is shifting toward the bid side. Because the best ask becomes relatively easier to exhaust while the best bid becomes harder to exhaust, the next price change may be more likely to shift upward, but it does not guarantee that the price will rise because orders may be executed or cancelled.
