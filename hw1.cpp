#include <cstdio>

struct Metrics {
    double mid;
    double spread;
    double micro;
    double obi;
};

Metrics compute(double bp, double bs, double ap, double as) {
    Metrics result;

    result.mid = (bp + ap) / 2.0;
    result.spread = ap - bp;
    if (ap == bp) {
        printf("Warning: locked book (ask price equals bid price).\n");
    }
    else if (ap < bp) {
        printf("Warning: crossed book (ask price is below bid price).\n");
    }
    result.micro = (ap * bs + bp * as) / (bs + as);
    result.obi = (bs - as) / (bs + as);

    return result;
}

struct Level {
    double price;
    double size;
};

Level find_best_bid(double prices[], double sizes[], int count) {
    Level best;

    best.price = prices[0];
    best.size = sizes[0];

    for (int i = 1; i < count; ++i) {
        if (prices[i] > best.price) {
            best.price = prices[i];
            best.size = sizes[i];
        }
    }

    return best;
}

Level find_best_ask(double prices[], double sizes[], int count) {
    Level best;

    best.price = prices[0];
    best.size = sizes[0];

    for (int i = 1; i < count; ++i) {
        if (prices[i] < best.price) {
            best.price = prices[i];
            best.size = sizes[i];
        }
    }

    return best;
}

void analyze_snapshot(
    double bid_prices[],
    double bid_sizes[],
    double ask_prices[],
    double ask_sizes[],
    int count,
    int snapshot_number
) {
    Level best_bid = find_best_bid(bid_prices, bid_sizes, count);
    Level best_ask = find_best_ask(ask_prices, ask_sizes, count);

    Metrics metrics = compute(
        best_bid.price,
        best_bid.size,
        best_ask.price,
        best_ask.size
    );

    printf("Snapshot %d\n", snapshot_number);
    printf("Best bid: price=%.2f quantity=%.0f\n",
           best_bid.price, best_bid.size);
    printf("Best ask: price=%.2f quantity=%.0f\n",
           best_ask.price, best_ask.size);
    printf("mid=%.4f spread=%.4f micro=%.4f obi=%.4f\n",
           metrics.mid,
           metrics.spread,
           metrics.micro,
           metrics.obi);
}

int main() {
    Metrics balanced = compute(100.00, 500, 100.02, 500);
    printf("\nBalanced case\n");
    printf("mid=%.4f spread=%.4f micro=%.4f obi=%.4f\n",
           balanced.mid,
           balanced.spread,
           balanced.micro,
           balanced.obi);

    Metrics bid_heavy = compute(100.00, 900, 100.02, 100);
    printf("\nBid-heavy case\n");
    printf("mid=%.4f spread=%.4f micro=%.4f obi=%.4f\n",
           bid_heavy.mid,
           bid_heavy.spread,
           bid_heavy.micro,
           bid_heavy.obi);

    Metrics ask_heavy = compute(100.00, 100, 100.02, 900);
    printf("\nAsk-heavy case\n");
    printf("mid=%.4f spread=%.4f micro=%.4f obi=%.4f\n",
           ask_heavy.mid,
           ask_heavy.spread,
           ask_heavy.micro,
           ask_heavy.obi);

    printf("\nLocked case\n");
    Metrics locked = compute(100.00, 500, 100.00, 500);
    printf("mid=%.4f spread=%.4f micro=%.4f obi=%.4f\n",
        locked.mid,
        locked.spread,
        locked.micro,
        locked.obi);

    printf("\nCrossed case\n");
    Metrics crossed = compute(100.02, 500, 100.00, 500);
    printf("mid=%.4f spread=%.4f micro=%.4f obi=%.4f\n",
        crossed.mid,
        crossed.spread,
        crossed.micro,
        crossed.obi);

    // Five constructed order-book snapshots
    printf("\nSnapshot sequence\n");

    const int snapshot_count = 5;
    const int level_count = 3;

    double sequence_bid_prices[snapshot_count][level_count] = {
        {99.98, 99.99, 99.97},
        {99.98, 99.99, 99.97},
        {99.98, 99.99, 99.97},
        {99.98, 99.99, 99.97},
        {100.00, 100.01, 99.99}
    };

    double sequence_bid_sizes[snapshot_count][level_count] = {
        {300, 200, 400},
        {300, 350, 400},
        {300, 500, 400},
        {300, 700, 400},
        {300, 800, 400}
    };

    double sequence_ask_prices[snapshot_count][level_count] = {
        {100.03, 100.01, 100.02},
        {100.03, 100.01, 100.02},
        {100.03, 100.01, 100.02},
        {100.03, 100.01, 100.02},
        {100.05, 100.03, 100.04}
    };

    double sequence_ask_sizes[snapshot_count][level_count] = {
        {500, 800, 600},
        {500, 650, 600},
        {500, 500, 600},
        {500, 300, 600},
        {500, 200, 600}
    };

    for (int i = 0; i < snapshot_count; ++i) {
        printf("\n");

        analyze_snapshot(
            sequence_bid_prices[i],
            sequence_bid_sizes[i],
            sequence_ask_prices[i],
            sequence_ask_sizes[i],
            level_count,
            i + 1
        );
    }

    return 0;
}