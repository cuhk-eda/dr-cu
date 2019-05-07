Dr. CU
======================================
Dr. CU is a VLSI detailed routing tool developed by the research team supervised by Prof. Evangeline F. Y. Young in The Chinese University of Hong Kong (CUHK). Different from global routing, detailed routing takes care of many detailed design rules and is performed on a significantly larger routing grid graph. In advanced technology nodes, it becomes the most complicated and time-consuming stage in the VLSI physical design flow. To tackle the chanllenges, we design and implement several efficient and effective data structures and algorithms under a holistic framework:
* A set of two-level sparse data structures
* An optimal correct-by-construction path search
* An efficient bulk synchronous parallel scheme
* ...

More details are in the following papers:

* Gengjie Chen, Chak-Wa Pui, Haocheng Li, Jingsong Chen, Bentian Jiang and Evangeline F. Y. Young,
["Detailed Routing by Sparse Grid Graph and Minimum-Area-Captured Path Search"](https://doi.org/10.1145/3287624.3287678),
IEEE/ACM Asia and South Pacific Design Automation Conference (ASPDAC), Tokyo, Japan, Jan 21-24, 2019.
* Gengjie Chen, Chak-Wa Pui, Haocheng Li and Evangeline F. Y. Young,
"Dr. CU: Detailed Routing by Sparse Grid Graph and Minimum-Area-Captured Path Search",
submitted to IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems (TCAD).

This version of code supports [ISPD'18 benchmarks](http://www.ispd.cc/contests/18/#benchmarks) and is up to a recent submission to TCAD. A more updated version with the adaption to [ISPD'19 benchmarks](http://www.ispd.cc/contests/19/#benchmarks) and some further enhancement will be released later.

## Compile

```bash
$ cd dr-cu
$ ./scripts/build.py -o release
```

## Run

```bash
$ cd dr-cu/run
$ ./run.py <benchmark_name...|all> [option...]
```

## Results

Experiments are performed on a 64-bit Linux workstation with Intel Xeon Silver 4114 CPU (2.20GHz, 40 cores) and 256GB memory. Benchmarks and metric weights for the total quality score are from [ISPD'18 Contest](http://www.ispd.cc/contests/18). Consistent with the contest, eight threads are used. The result reporting is conducted by Cadence Innovus 17.1 and the official evaluation script.

| design            | WL       | # vias  | og WL   | og # vias | ot WL | ot # vias | ww WL  | short area | # min area | # space | total score  | mem (GB)  | time (s) |
|-------------------|----------|---------|---------|-----------|-------|-----------|--------|------------|------------|---------|--------------|-----------|----------|
| **ispd18_test1**  | 433254   | 32031   | 1706    | 446       | 393   | 0         | 4749   | 0.4        | 0          | 17      | **296504**   | **0.33**  | **11**   |
| **ispd18_test2**  | 7806294  | 317160  | 34194   | 5948      | 4937  | 0         | 44495  | 1.3        | 0          | 73      | **4661740**  | **1.70**  | **85**   |
| **ispd18_test3**  | 8683731  | 307545  | 52408   | 5499      | 5714  | 0         | 45541  | 372.5      | 0          | 161     | **5330014**  | **1.75**  | **113**  |
| **ispd18_test4**  | 26033480 | 658644  | 132938  | 16103     | 9190  | 0         | 59579  | 436.8      | 6          | 1071    | **15304156** | **3.94**  | **320**  |
| **ispd18_test5**  | 27729394 | 916715  | 92872   | 16686     | 1588  | 0         | 44680  | 77.4       | 10         | 496     | **16144832** | **5.42**  | **426**  |
| **ispd18_test6**  | 35595790 | 1403634 | 142595  | 25939     | 8735  | 0         | 69829  | 92.7       | 21         | 587     | **21198243** | **6.48**  | **527**  |
| **ispd18_test7**  | 64994186 | 2271738 | 235497  | 36269     | 16459 | 0         | 106884 | 230.8      | 38         | 325     | **37724327** | **10.77** | **969**  |
| **ispd18_test8**  | 65289434 | 2281513 | 290418  | 38596     | 17082 | 0         | 111173 | 249.5      | 20         | 399     | **37990696** | **11.73** | **1034** |
| **ispd18_test9**  | 54602832 | 2282226 | 284645  | 42078     | 12746 | 0         | 108324 | 162.7      | 28         | 379     | **32592136** | **11.20** | **906**  |
| **ispd18_test10** | 67907614 | 2439531 | 1137257 | 64535     | 30527 | 0         | 197840 | 11370.4    | 44         | 3910    | **47909940** | **11.95** | **1299** |