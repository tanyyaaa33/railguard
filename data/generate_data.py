#!/usr/bin/env python3
"""Generate synthetic RailGuard CSVs: 50 stations, 30 trains, ~180 days delay logs."""
from __future__ import annotations

import csv
import math
import os
import random
from datetime import date, timedelta

random.seed(42)

# ~50 real North / Central India stations (subset), approximate lat/lon
STATIONS = [
    (0, "New Delhi", "NR", 28.6139, 77.2090),
    (1, "Old Delhi", "NR", 28.6550, 77.2300),
    (2, "Ambala Cantt", "NR", 30.3783, 76.7767),
    (3, "Ludhiana", "NR", 30.9010, 75.8573),
    (4, "Jalandhar City", "NR", 31.3260, 75.5762),
    (5, "Amritsar", "NR", 31.6340, 74.8723),
    (6, "Mathura Junction", "NCR", 27.4924, 77.6737),
    (7, "Agra Cantt", "NCR", 27.1575, 77.9919),
    (8, "Pt DD Upadhyaya Jn", "ECR", 25.4500, 83.1200),  # Mughal Sarai area
    (9, "Varanasi Junction", "NER", 25.3317, 82.9876),
    (10, "Lucknow NR", "NER", 26.8320, 80.9243),
    (11, "Kanpur Central", "NCR", 26.4550, 80.3200),
    (12, "Itarsi Junction", "CR", 22.6140, 77.7620),
    (13, "Bhopal Junction", "WCR", 23.2599, 77.4126),
    (14, "Jhansi Junction", "NCR", 25.4480, 78.5680),
    (15, "Gwalior Junction", "NCR", 26.2183, 78.1828),
    (16, "Kota Junction", "WCR", 25.2139, 75.8648),
    (17, "Jaipur Junction", "NWR", 26.9124, 75.7873),
    (18, "Ajmer Junction", "NWR", 26.4499, 74.6399),
    (19, "Ratlam Junction", "WR", 23.3340, 75.0372),
    (20, "Vadodara Junction", "WR", 22.3100, 73.1800),
    (21, "Surat", "WR", 21.1950, 72.8190),
    (22, "Mumbai Central", "WR", 18.9717, 72.8194),
    (23, "Pune Junction", "CR", 18.5286, 73.8742),
    (24, "Nagpur Junction", "CR", 21.1490, 79.0890),
    (25, "Balharshah", "CR", 19.8500, 79.3500),
    (26, "Secunderabad Junction", "SCR", 17.4399, 78.4983),
    (27, "Kacheguda", "SCR", 17.3898, 78.5020),
    (28, "Patna Junction", "ECR", 25.6020, 85.1372),
    (29, "Gaya Junction", "ECR", 24.7970, 84.9990),
    (30, "Dhanbad Junction", "ECR", 23.7957, 86.4304),
    (31, "Asansol Junction", "ER", 23.6739, 86.9524),
    (32, "Howrah", "ER", 22.5823, 88.3426),
    (33, "Kharagpur Junction", "SER", 22.3302, 87.3237),
    (34, "Bilaspur Junction", "SECR", 22.0796, 82.1391),
    (35, "Raipur Junction", "SECR", 21.2514, 81.6296),
    (36, "Rourkela", "SER", 22.2604, 84.8536),
    (37, "Tatanagar", "SER", 22.7697, 86.2008),
    (38, "Ranchi", "SER", 23.3490, 85.3345),
    (39, "Prayagraj Junction", "NCR", 25.4358, 81.8463),
    (40, "Bareilly Junction", "NER", 28.3670, 79.4304),
    (41, "Moradabad", "NR", 28.8386, 78.7738),
    (42, "Saharanpur", "NR", 29.9680, 77.5451),
    (43, "Meerut City", "NR", 28.9800, 77.7064),
    (44, "Aligarh Junction", "NCR", 27.8974, 78.0880),
    (45, "Tundla Junction", "NCR", 27.2000, 78.2330),
    (46, "Etawah Junction", "NCR", 26.7760, 79.0210),
    (47, "Mughalsarai Yard Ref", "ECR", 25.4500, 83.1200),
    (48, "Manmad Junction", "CR", 20.2500, 74.4400),
    (49, "Kalyan Junction", "CR", 19.2433, 73.1355),
]

JUNCTION_HIGH_VARIANCE = {8, 11, 12, 39, 47}  # DD Upadhyaya, Kanpur, Itarsi, Prayagraj, Mughalsarai ref

BASE = date(2024, 4, 1)
DAYS = 180


def skew_delay(rng: random.Random, station_id: int, day_index: int) -> int:
    """Right-skewed delay (lognormal-ish via exp of normal), junction boost, peak months."""
    month = (BASE + timedelta(days=day_index)).month
    peak = 1.35 if month in (10, 11, 3) else 1.0
    u = max(1e-6, rng.random())
    base = math.exp(rng.gauss(0.8, 0.65)) * 8.0 * peak
    if station_id in JUNCTION_HIGH_VARIANCE:
        base *= rng.uniform(1.2, 2.2)
        base += rng.expovariate(1.0 / 25.0)
    return int(min(600, max(0, round(base + rng.gauss(0, 3)))))


def main() -> None:
    root = os.path.dirname(os.path.abspath(__file__))
    os.chdir(root)

    with open("stations.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["station_id", "name", "zone", "lat", "lon"])
        for sid, name, zone, la, lo in STATIONS:
            w.writerow([sid, name, zone, f"{la:.6f}", f"{lo:.6f}"])

    # Backbone edges (undirected for graph) — distances approx km, capacity passengers/day edge
    edges_set = set()
    def add_edge(a, b, dist, cap):
        if a > b:
            a, b = b, a
        if (a, b) not in edges_set:
            edges_set.add((a, b))
            routes_rows.append([a, b, dist, cap])

    routes_rows = []
    chain = [0, 1, 6, 7, 45, 11, 39, 8, 9, 28, 29]
    for i in range(len(chain) - 1):
        add_edge(chain[i], chain[i + 1], random.randint(80, 220), random.randint(8000, 22000))
    add_edge(11, 12, 650, 18000)
    add_edge(12, 13, 740, 16000)
    add_edge(12, 24, 420, 15000)
    add_edge(24, 25, 240, 12000)
    add_edge(22, 49, 48, 25000)
    add_edge(49, 23, 120, 20000)
    add_edge(13, 16, 320, 14000)
    add_edge(16, 17, 240, 13000)
    add_edge(17, 18, 150, 11000)
    add_edge(0, 2, 195, 15000)
    add_edge(2, 3, 100, 12000)
    add_edge(3, 4, 55, 10000)
    add_edge(4, 5, 80, 11000)
    add_edge(1, 44, 140, 14000)
    add_edge(14, 15, 100, 10000)
    add_edge(20, 21, 130, 16000)
    add_edge(31, 32, 200, 22000)
    add_edge(28, 30, 250, 10000)
    add_edge(35, 34, 120, 9000)
    add_edge(8, 47, 5, 50000)
    add_edge(23, 48, 240, 10000)
    add_edge(48, 12, 260, 11000)
    for _ in range(25):
        a, b = random.sample(range(50), 2)
        if a != b:
            add_edge(a, b, random.randint(40, 400), random.randint(4000, 14000))

    with open("routes.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["from_id", "to_id", "distance_km", "capacity"])
        for row in sorted(routes_rows, key=lambda r: (r[0], r[1])):
            w.writerow(row)

    # 30 trains: routes as station id lists along backbone + spurs
    train_templates = [
        ("Rajdhani NDLS-ALD", [0, 1, 6, 7, 11, 39, 8, 9]),
        ("Shatabdi NDLS-BPL", [0, 1, 44, 11, 12, 13]),
        ("Duronto NDLS-HWH", [0, 11, 12, 24, 31, 32]),
        ("Mail NDLS-PNBE", [1, 44, 11, 39, 28]),
        ("Express BCT-NDLS", [22, 49, 23, 48, 12, 11, 0]),
        ("Superfast LKO-Varanasi", [10, 11, 39, 9]),
        ("Passenger JHS-BPL", [14, 15, 11, 12, 13]),
        ("Corridor Ref DD Upadhyaya", [8, 47, 12]),
        ("Express JP-DEE", [17, 16, 13, 12, 0]),
        ("Night Rider NDLS-KOTA", [0, 11, 16]),
        ("Garib Rath ADI-PNBE", [20, 12, 11, 28]),
        ("Humsafar BCT-Varanasi", [22, 49, 12, 8, 9]),
        ("Antyodaya RNC-NDLS", [38, 30, 11, 0]),
        ("Tejas NDLS-AGC", [0, 6, 7]),
        ("Double Decker NDLS-LKO", [0, 11, 10]),
        ("Jan Shatabdi BPL-Itarsi", [13, 12]),
        ("Intercity Kota-Ratlam", [16, 19, 12]),
        ("Passenger GWL-BSB", [15, 11, 39, 9]),
        ("Express ASN-TATA", [31, 37, 24]),
        ("Superfast Pune-Nagpur", [23, 24]),
    ]
    rng = random.Random(123)
    trains_out = []
    for tid in range(30):
        if tid < len(train_templates):
            name, route = train_templates[tid]
        else:
            name = f"Regional_{tid}"
            start = rng.randint(0, 49)
            route = [start]
            for _ in range(rng.randint(4, 10)):
                nxt = rng.randint(0, 49)
                if nxt != route[-1]:
                    route.append(nxt)
        sched = ";".join(str((i * 47 + tid * 3) % 1440) for i in range(len(route)))
        trains_out.append([tid, name, ";".join(map(str, route)), sched])

    with open("trains.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["train_id", "name", "route", "schedule_minutes"])
        for row in trains_out:
            w.writerow(row)

    delay_rows = []
    for day in range(DAYS):
        d = BASE + timedelta(days=day)
        ds = d.isoformat()
        for train_id in range(30):
            route_str = trains_out[train_id][2]
            sts = [int(x) for x in route_str.split(";")]
            for st in sts:
                dm = skew_delay(rng, st, day)
                delay_rows.append([train_id, st, ds, dm])

    with open("delays.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["train_id", "station_id", "date", "delay_minutes"])
        for row in delay_rows:
            w.writerow(row)

    print("Wrote stations.csv, routes.csv, trains.csv, delays.csv")


if __name__ == "__main__":
    main()
