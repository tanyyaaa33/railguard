# RailGuard

RailGuard is a **C++17** command-line tool that models Indian Railways infrastructure as a **weighted directed graph**, simulates **real-time cascade delays** on a precedence DAG of train–station events, identifies delay **hotspots** from 180 days of historical data, and proposes **intervention plans** using a greedy passenger-impact model with Sprague–Grundy game-theoretic scoring of cascade states.

**Stack:** C++17, CMake >= 3.16, STL only — no third-party libraries.  
**Compiler flags:** `-Wall -Wextra -O2`

---

## Repository layout

```
railguard/
├── CMakeLists.txt                         # builds railguard_core (static lib) + 5 executables
├── README.md
├── visualizer/
│   ├── dashboard.py                       # Streamlit dashboard — 4 tabs
│   └── requirements.txt                   # streamlit pandas matplotlib networkx
├── data/
│   ├── generate_data.py                   # synthetic data generator (Python 3, stdlib only)
│   ├── stations.csv                       # 50 North/Central India stations
│   ├── routes.csv                         # backbone + spur directed edges
│   ├── trains.csv                         # 30 trains with routes + per-hop schedules
│   └── delays.csv                         # ~29 000 rows — 180 days of historical delays
├── src/
│   ├── main.cpp                           # CLI entry point — all five sub-commands
│   ├── graph/
│   │   ├── NetworkGraph.h
│   │   ├── NetworkGraph.cpp               # Station struct, undirected adjacency list, loadNetworkFromCsv()
│   │   ├── TarjanBridge.h
│   │   └── TarjanBridge.cpp               # runTarjan() → TarjanResult {bridges, articulation_points}
│   ├── cascade/
│   │   ├── DelayDAG.h
│   │   ├── DelayDAG.cpp                   # DelayDAG::propagateFromTrainDelay() → CascadeResult
│   │   ├── DSUWithRollback.h
│   │   └── DSUWithRollback.cpp            # DSUWithRollback: find() / unite() / rollback(k)
│   ├── analytics/
│   │   ├── SlidingWindow.h
│   │   ├── SlidingWindow.cpp              # SlidingWindowAnalyzer: hotspotsFixed(), fillVariableWindowStats()
│   │   ├── PassengerImpact.h
│   │   └── PassengerImpact.cpp            # PassengerModel: passengersOnTrain(), passengerDelayMinutes()
│   ├── intervention/
│   │   ├── SpragueGrundy.h
│   │   ├── SpragueGrundy.cpp              # analyzeCascadeNim(), delaysToHeaps(), mex(), xorSum(), nimGrundy()
│   │   ├── InterventionOptimizer.h
│   │   └── InterventionOptimizer.cpp      # optimizeInterventionsGreedy(), minInterventionsForThreshold()
│   └── io/
│       ├── CSVParser.h
│       ├── CSVParser.cpp                  # parseCsvLine() — RFC-4180-ish comma split
│       ├── NTESFeedSimulator.h
│       └── NTESFeedSimulator.cpp          # NTESFeedSimulator: load(), nextDay(), reset()
└── tests/
    ├── test_tarjan.cpp                    # path P4: asserts 3 bridges, 2 APs; disconnected variant
    ├── test_dsu.cpp                       # 5 unions, rollback(2), asserts 3 separate components
    ├── test_cascade.cpp                   # seed train >= initial delay; max > seed (compounding); delays vary
    └── test_sprague.cpp                   # Nim XOR correctness, mex(), delaysToHeaps() boundary values
```

---

## Build

### With CMake (recommended)

```bash
cd railguard
cmake -S . -B build
cmake --build build
```

Install CMake via Homebrew if needed:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
eval "$(/opt/homebrew/bin/brew shellenv)"
brew install cmake
```

### Without CMake (Apple Clang / g++ direct)

If CMake is not available, compile directly with `g++` (Apple Clang works):

```bash
cd railguard

g++ -std=c++17 -Wall -Wextra -O2 -Isrc -c \
  src/graph/NetworkGraph.cpp src/graph/TarjanBridge.cpp \
  src/cascade/DelayDAG.cpp src/cascade/DSUWithRollback.cpp \
  src/analytics/SlidingWindow.cpp src/analytics/PassengerImpact.cpp \
  src/intervention/SpragueGrundy.cpp src/intervention/InterventionOptimizer.cpp \
  src/io/CSVParser.cpp src/io/NTESFeedSimulator.cpp

ar rcs librailguard.a *.o
mkdir -p build
g++ -std=c++17 -Wall -Wextra -O2 -Isrc src/main.cpp librailguard.a -o build/railguard
```

This produces:

| Executable | Source |
|-----------|--------|
| `build/railguard` | `src/main.cpp` + `railguard_core` |
| `build/test_tarjan` | `tests/test_tarjan.cpp` |
| `build/test_dsu` | `tests/test_dsu.cpp` |
| `build/test_cascade` | `tests/test_cascade.cpp` |
| `build/test_sprague` | `tests/test_sprague.cpp` |

All link against **`railguard_core`** — a static library compiled from the ten source files in `src/`.

---

## Tests

```bash
ctest --test-dir build
```

Four registered tests:

| CTest name | Binary | What it checks |
|-----------|--------|----------------|
| `tarjan` | `test_tarjan` | Path `P4` (0-1-2-3): 3 bridges, 2 articulation points; re-run with 5-vertex disconnected graph |
| `dsu_rollback` | `test_dsu` | 5 unions over 6 nodes, `rollback(2)`, asserts three separate components |
| `cascade` | `test_cascade` | `propagateFromTrainDelay(0, 25)`: seed train carries >= seed delay; at least one train exceeds seed (compounding); delay values are not all equal (variation) |
| `sprague` | `test_sprague` | XOR-of-{1,2,3}=0 (critical), XOR-of-{1,2}!=0 (recoverable), `mex({0,1,2})`=3, `delaysToHeaps` edge cases |

`RAILGUARD_DATA` is set to `${CMAKE_SOURCE_DIR}/data` automatically for the `cascade` test by `CMakeLists.txt`.

---

## Data directory

By default, the CLI resolves data files relative to the current working directory as `./data/`. Override:

```bash
export RAILGUARD_DATA=/absolute/path/to/railguard/data
```

### CSV schemas

**`stations.csv`**
```
station_id,name,zone,lat,lon
0,New Delhi,NR,28.613900,77.209000
...
```

**`routes.csv`**
```
from_id,to_id,distance_km,capacity
0,1,105,14821
...
```

**`trains.csv`**  
`route` and `schedule_minutes` are `;`-delimited lists of integers.
```
train_id,name,route,schedule_minutes
0,Rajdhani NDLS-ALD,0;1;6;7;11;39;8;9,0;47;94;141;188;235;282;329
...
```

**`delays.csv`**
```
train_id,station_id,date,delay_minutes
0,0,2024-04-01,13
...
```

### Regenerating synthetic data

```bash
cd data
python3 generate_data.py
```

Writes all four CSVs. Statistics: 50 stations, 30 trains, 180 days (`2024-04-01` to `2024-09-27`).  
Delay distribution: right-skewed (lognormal base); junctions Itarsi (12), Kanpur (11), Prayagraj (39), DD Upadhyaya (8/47) have higher variance; peak months Oct–Nov and Mar carry a 1.35× multiplier.

---

## CLI usage

Run from a directory that contains `data/`, or set `RAILGUARD_DATA`.

```bash
export RAILGUARD_DATA=/path/to/railguard/data
./build/railguard <command> [flags]
```

### `--analyze-network`

Loads `stations.csv` + `routes.csv` into an undirected graph, then runs **Tarjan** to find all bridge edges and articulation-point stations.

```bash
railguard --analyze-network
```

Output: sorted bridge list `(u v)` with `u < v`, then articulation point ids with station names.

### `--cascade <train_id> <delay_minutes>`

Seeds an initial delay at the route origin of the given train and propagates it through the **delay DAG** (platform queue + along-route precedence). Prints every affected `(train_id, propagated_delay_min)` pair.

```bash
railguard --cascade 0 15
```

### `--hotspots --window <days> [--threshold <T>]`

Aggregates `delays.csv` into per-station daily averages, then ranks stations by the **mean of the last `W` days**. Default threshold `T = 35` minutes; stations above it are marked `HOT`. Prints top 10 plus a **variable-window** statistic (maximum mean over any contiguous day span in history).

```bash
railguard --hotspots --window 7
railguard --hotspots --window 14 --threshold 50
```

### `--intervene <train_id> --budget <K> [--seed-delay <m>] [--pdm-threshold <T>]`

Builds a cascade from the seed train (default seed delay **60 minutes**; override with `--seed-delay`). Selects up to **K** trains **greedily** by descending passenger-delay-minute (PDM) reduction — each chosen intervention clears that train's delay to zero. Optionally finds the **minimum k ≤ K** that brings total PDM below `--pdm-threshold` via binary search.

```bash
railguard --intervene 5 --budget 3
railguard --intervene 2 --budget 5 --seed-delay 45 --pdm-threshold 500000
```

### `--game-state <cascade_id>`

`cascade_id` format: `trainId_initialDelay` (e.g. `5_40`). Builds the cascade, maps each affected train's delay to a Nim **heap size** (`ceil(delay / 15)`), XORs all heap sizes (Sprague–Grundy for Nim), and prints the **Grundy value** plus a verdict:

- **recoverable** — Grundy XOR != 0 (first mover / controller has a winning strategy)
- **critical** — Grundy XOR == 0 (cascade "wins"; intervention required)

```bash
railguard --game-state 0_20
# cascade_id=0_20 heaps=2,2,...  Grundy XOR=0 verdict=critical
```

---

## Module reference

### `src/graph/NetworkGraph.h/.cpp`

- **`struct Station`** — `id`, `name`, `zone`, `lat`, `lon`
- **`class NetworkGraph`** — undirected adjacency list; `setNumVertices(n)`, `addUndirectedEdge(u,v)`, `adjacency()`, `stations`
- **`loadNetworkFromCsv(dataDir, graph, err)`** — reads `stations.csv` and `routes.csv`

### `src/graph/TarjanBridge.h/.cpp`

- **`struct TarjanResult`** — `bridges` (sorted pairs), `articulation_points` (sorted ids)
- **`runTarjan(graph)`** — O(V+E); multi-component DFS (restarts for every unvisited vertex)
- Citations: Tarjan 1974, CLRS §22.3, [cp-algorithms/bridge-searching](https://cp-algorithms.com/graph/bridge-searching.html), [cp-algorithms/cutpoints](https://cp-algorithms.com/graph/cutpoints.html)

### `src/cascade/DelayDAG.h/.cpp`

- **`struct TrainRoute`** — `train_id`, `name`, `stations` (vector), `schedule_mins` (vector)
- **`struct CascadeResult`** — `ok`, `error`, `topo_order`, `node_delay`, `affected_trains`
- **`class DelayDAG`** — `loadTrainsCsv(dataDir, err)`, `propagateFromTrainDelay(train_id, initial_delay)`
- Internally: nodes = (train index, hop index); two edge types with explicit weights:
  - **Along-route** `(ti,h) → (ti,h+1)`, weight = 0 — delay carries forward unchanged, train cannot recover time en route
  - **Platform-conflict** `(A at S) → (B at S)`, weight = `platformTurnaround(S) − schedule_gap(A,B)` — positive weight compounds delay (tight gap); negative weight absorbs it (wide buffer); turnaround = 15–25 min varied by station id
- Propagation rule: `delay[v] = max(delay[v], max(0, delay[u] + weight))` when `delay[u] > 0`
- **Kahn's algorithm** — O(V+E) topo sort; then O(V+E) weighted DAG relaxation in topo order (longest-path style)
- Produces realistic compounding: e.g. `--cascade 0 30` yields delays ranging 23–105 min across affected trains
- Citations: CLRS §22.4, [cp-algorithms/topological-sort](https://cp-algorithms.com/graph/topological-sort.html)

### `src/cascade/DSUWithRollback.h/.cpp`

- **`class DSUWithRollback`** — `find(x)`, `unite(a,b)` (returns bool), `rollback(k)`, `stackDepth()`
- Union by rank; **no path compression** (required for rollback correctness)
- `find`: O(log n); `rollback(k)`: O(k) stack pops with parent + rank restore
- Citations: CLRS §21.3, [cp-algorithms/dsu](https://cp-algorithms.com/data_structures/disjoint_set_union.html)

### `src/analytics/SlidingWindow.h/.cpp`

- **`struct StationHotspot`** — `station_id`, `name`, `fixed_window_mean`, `variable_window_mean`, `variable_best_len`, `days_in_fixed`
- **`class SlidingWindowAnalyzer`** — `load(dataDir, err)`, `hotspotsFixed(W, T, limit)`, `fillVariableWindowStats(rows)`
- Fixed window: last `W` daily-average points per station — O(S·W)
- Variable window: max-mean over any contiguous span — O(D²) per station via prefix sums (D ≤ 180)

### `src/analytics/PassengerImpact.h/.cpp`

- **`struct TrainMeta`** — `train_id`, `name`, `route_len`
- **`class PassengerModel`** — `loadTrainsCsv(dataDir, err)`, `passengersOnTrain(train_id)`, `passengerDelayMinutes(train_id, delay)`
- Load formula: `650 + 45 * route_stops + 12 * train_id` (deterministic, no CSV passenger column)
- **`totalPassengerDelay(train_delay_pairs, model)`** — aggregate helper

### `src/intervention/SpragueGrundy.h/.cpp`

- **`struct GameAnalysis`** — `grundy_value`, `recoverable`, `verdict`
- **`analyzeCascadeNim(heap_sizes)`** — XOR all Nim heap Grundy numbers; O(n)
- **`delaysToHeaps(delays, chunk=15)`** — maps delay minutes to heap sizes; d ≤ 0 → heap 0
- **`mex(reachable_grundies)`** — minimum excludant for extension to non-Nim games
- **`nimGrundy(n)`**, **`xorSum(values)`**
- Citations: Berlekamp/Conway/Guy "Winning Ways", [cp-algorithms/sprague-grundy-nim](https://cp-algorithms.com/game_theory/sprague-grundy-nim.html)

### `src/intervention/InterventionOptimizer.h/.cpp`

- **`struct InterventionPlan`** — `chosen_trains`, `passenger_delay_before`, `passenger_delay_after`
- **`optimizeInterventionsGreedy(affected, pax, K)`** — O(T log T) sort by marginal PDM; picks top K
- **`minInterventionsForThreshold(affected, pax, max_k, threshold)`** — binary search on k ∈ [0, max_k]; valid because PDM is monotone non-increasing in k under greedy policy; O(log K · T log T)

### `src/io/CSVParser.h/.cpp`

- **`parseCsvLine(line)`** — returns `vector<string>`; comma-split without embedded-comma support (sufficient for synthetic data)

### `src/io/NTESFeedSimulator.h/.cpp`

- **`struct NtesRow`** — `train_id`, `station_id`, `date`, `delay_minutes`
- **`class NTESFeedSimulator`** — `load(dataDir, err)`, `nextDay(batch, day_iso)`, `reset()`, `currentDate()`
- Sorts all rows by date on load — O(N log N); `nextDay()` returns one calendar-day batch — O(batch size)
- Not wired to the main CLI; intended for programmatic offline replay

---

## Algorithm complexity summary

| Module | Algorithm | Complexity | Citation |
|--------|-----------|-----------|---------|
| `TarjanBridge.cpp` | DFS bridges + APs | O(V+E) | Tarjan 1974; CLRS §22.3 |
| `DelayDAG.cpp` | Kahn topo sort + DAG relaxation | O(V+E) | CLRS §22.4 |
| `DSUWithRollback.cpp` | Union-by-rank, no path compression | find O(log n); rollback O(k) | CLRS §21.3 |
| `SlidingWindow.cpp` | Prefix-sum window means | O(S·W) fixed; O(D²) variable | — |
| `SpragueGrundy.cpp` | Nim XOR Grundy | O(n) | Sprague–Grundy theorem |
| `InterventionOptimizer.cpp` | Greedy + binary search | O(T log T) + O(log K · T log T) | CLRS §2.3–2.4 |

---

## Environment variable

| Variable | Default | Purpose |
|----------|---------|---------|
| `RAILGUARD_DATA` | `./data` | Path to directory containing the four CSV files |

---

## Visualizer dashboard

A self-contained Streamlit dashboard lives in `visualizer/dashboard.py`. It reads the same four CSV files and calls the `railguard` binary via subprocess for all live computations.

### Prerequisites

Build the C++ binary first (see [Build](#build) above), then install the Python dependencies.

**Using a virtual environment (recommended — avoids macOS system-Python restrictions):**

```bash
cd visualizer
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

**Or with pip3 directly:**

```bash
cd visualizer
pip3 install -r requirements.txt
```

`requirements.txt` contains: `streamlit`, `pandas`, `matplotlib`, `networkx`.

### Running

```bash
cd visualizer
source venv/bin/activate   # if using a venv
streamlit run dashboard.py
```

The dashboard opens in your browser at `http://localhost:8501`.

If the data directory is not `../data` relative to the `visualizer/` folder, set:

```bash
export RAILGUARD_DATA=/absolute/path/to/railguard/data
streamlit run dashboard.py
```

Each subsequent terminal session: `source venv/bin/activate` then `streamlit run dashboard.py`.

### Tabs

| Tab | What it shows |
|-----|---------------|
| **Network Map** | Station graph drawn with geographic coordinates (lat/lon). Bridge edges in orange, articulation-point stations in red, normal stations in blue. Calls `--analyze-network`. |
| **Cascade Simulator** | Select a train (0–29) and initial delay (10–120 min). Runs `--cascade`, plots a bar chart of propagated delay per affected train, and reports total passengers affected and total passenger-delay-minutes. |
| **Delay Hotspots** | Sliding window (7–30 days). Runs `--hotspots`, plots top-10 stations by mean daily delay. HOT stations (above threshold) are red, others blue. |
| **Intervention Planner** | Select train, budget K (1–10), and seed delay. Runs `--intervene`, shows PDM before vs after as a comparison bar chart, lists chosen trains, and displays % PDM reduction as a headline metric. |
