# strings performance

`bench/bench.cc` times `lower`, `upper`, `join` and `replace` against abseil, in ns per call (µs and ms
where marked), the best of seven samples of at least 8 ms. Each row first checks that both return the
same string. The ratio is strings' time over abseil's, so below 1 is faster. Apple clang 17, `-O3`,
on an Apple M4 Pro (macOS 26).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSTRINGS_BUILD_BENCH=ON
cmake --build build --target strings_bench
./build/strings_bench
```

## lower, upper and their in-place forms

| Operation | Apple M4 Pro: strings | abseil | ratio |
| --- | ---: | ---: | ---: |
| lower(string_view) 1 B | 2.3 ns | 3.8 ns | 0.61 |
| upper(string_view) 1 B | 2.3 ns | 3.4 ns | 0.68 |
| inplace_lower 1 B | 2.1 ns | 2.7 ns | 0.78 |
| inplace_upper 1 B | 2.0 ns | 2.7 ns | 0.74 |
| lower(string_view) 2 B | 2.8 ns | 4.0 ns | 0.70 |
| upper(string_view) 2 B | 2.8 ns | 4.0 ns | 0.70 |
| inplace_lower 2 B | 3.4 ns | 3.7 ns | 0.92 |
| inplace_upper 2 B | 3.7 ns | 3.6 ns | 1.03 |
| lower(string_view) 4 B | 4.7 ns | 4.2 ns | 1.12 |
| upper(string_view) 4 B | 4.7 ns | 4.3 ns | 1.09 |
| inplace_lower 4 B | 3.4 ns | 3.7 ns | 0.92 |
| inplace_upper 4 B | 3.7 ns | 3.6 ns | 1.03 |
| lower(string_view) 8 B | 3.3 ns | 4.9 ns | 0.67 |
| upper(string_view) 8 B | 3.3 ns | 4.7 ns | 0.70 |
| inplace_lower 8 B | 2.5 ns | 4.1 ns | 0.61 |
| inplace_upper 8 B | 2.3 ns | 4.3 ns | 0.53 |
| lower(string_view) 16 B | 4.6 ns | 7.8 ns | 0.59 |
| upper(string_view) 16 B | 4.6 ns | 7.8 ns | 0.59 |
| inplace_lower 16 B | 2.8 ns | 6.8 ns | 0.41 |
| inplace_upper 16 B | 2.7 ns | 7.1 ns | 0.38 |
| lower(string_view) 32 B | 17.8 ns | 24.9 ns | 0.71 |
| upper(string_view) 32 B | 18.0 ns | 25.3 ns | 0.71 |
| inplace_lower 32 B | 3.5 ns | 11.2 ns | 0.31 |
| inplace_upper 32 B | 3.7 ns | 11.6 ns | 0.32 |
| lower(string_view) 256 B | 24.5 ns | 115 ns | 0.21 |
| upper(string_view) 256 B | 25.4 ns | 114 ns | 0.22 |
| inplace_lower 256 B | 6.5 ns | 84.0 ns | 0.08 |
| inplace_upper 256 B | 6.6 ns | 84.5 ns | 0.08 |
| lower(string_view) 4096 B | 128 ns | 1.35 µs | 0.10 |
| upper(string_view) 4096 B | 124 ns | 1.23 µs | 0.10 |
| inplace_lower 4096 B | 68.9 ns | 1.17 µs | 0.06 |
| inplace_upper 4096 B | 68.7 ns | 1.19 µs | 0.06 |

## join

| Operation | Apple M4 Pro: strings | abseil | ratio |
| --- | ---: | ---: | ---: |
| join(vector<string>) 1 | 4.5 ns | 5.0 ns | 0.90 |
| join(vector<int>) 1 | 7.3 ns | 8.4 ns | 0.87 |
| join(vector<string>) 2 | 10.0 ns | 8.8 ns | 1.14 |
| join(vector<int>) 2 | 14.9 ns | 15.0 ns | 0.99 |
| join(vector<string>) 3 | 28.1 ns | 24.9 ns | 1.13 |
| join(vector<int>) 3 | 23.9 ns | 22.9 ns | 1.04 |
| join(vector<string>) 4 | 33.8 ns | 29.6 ns | 1.14 |
| join(vector<int>) 4 | 51.0 ns | 60.3 ns | 0.85 |
| join(vector<string>) 10 | 60.8 ns | 48.2 ns | 1.26 |
| join(vector<int>) 10 | 121 ns | 128 ns | 0.95 |
| join(vector<string>) 1000 | 4.93 µs | 3.75 µs | 1.31 |
| join(vector<int>) 1000 | 8.34 µs | 8.97 µs | 0.93 |
| join(vector<string>) 1 x 60 B | 17.8 ns | 18.1 ns | 0.98 |
| join(vector<string>) 2 x 60 B | 25.0 ns | 23.9 ns | 1.05 |
| join(vector<string>) 3 x 60 B | 32.1 ns | 32.1 ns | 1.00 |

## replace

| Operation | Apple M4 Pro: strings | abseil | ratio |
| --- | ---: | ---: | ---: |
| replace 1->3, 16 B, 2 matches | 29.3 ns | 47.4 ns | 0.62 |
| replace 1->3, 32 B, 4 matches | 85.0 ns | 107 ns | 0.79 |
| replace 1->3, 256 B, 64 matches | 600 ns | 788 ns | 0.76 |
| replace 1->3, 256 B, 16 matches | 173 ns | 286 ns | 0.60 |
| replace 1->3, 256 B, 3 matches | 66.7 ns | 89.8 ns | 0.74 |
| replace 1->3, 256 B, 2 matches | 55.0 ns | 77.5 ns | 0.71 |
| replace 1->3, 256 B, 1 match | 38.5 ns | 63.6 ns | 0.61 |
| replace 1->3, 256 B, no match | 28.4 ns | 51.3 ns | 0.55 |
| replace 1->3, 4 KB, 1024 matches | 7.93 µs | 11.6 µs | 0.69 |
| replace 1->3, 4 KB, 1 match | 180 ns | 179 ns | 1.00 |
| replace 1->1, 256 B, 16 matches | 116 ns | 247 ns | 0.47 |
| replace 1->1, 256 B, 64 matches | 422 ns | 712 ns | 0.59 |
| replace 3->1, 256 B, 64 matches | 497 ns | 713 ns | 0.70 |
| replace 3->1, 256 B, 2 matches | 51.0 ns | 78.2 ns | 0.65 |
| replace 3->1, 4 KB, 1024 matches | 7.26 µs | 10.4 µs | 0.70 |
| replace 3->0, 256 B, 64 matches | 487 ns | 638 ns | 0.76 |
| replace 2->5, 256 B, 32 matches | 343 ns | 518 ns | 0.66 |
