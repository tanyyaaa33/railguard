"""
RailGuard Visualizer Dashboard
================================
Four-tab Streamlit dashboard for the RailGuard C++ CLI.

Tabs
----
1. Network Map        – station graph with bridge / articulation-point highlights
2. Cascade Simulator  – bar chart of propagated delays per train
3. Delay Hotspots     – sliding-window hotspot ranking
4. Intervention Planner – PDM before/after comparison + chosen trains

Usage
-----
    cd visualizer
    pip install -r requirements.txt
    streamlit run dashboard.py

The dashboard expects:
  - ../data/  containing stations.csv, routes.csv, trains.csv, delays.csv
  - ../build/railguard  (build first: cmake -S .. -B ../build && cmake --build ../build)

Set RAILGUARD_DATA env var to override the data directory.
"""

import os
import re
import subprocess
from pathlib import Path

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
import networkx as nx
import pandas as pd
import streamlit as st

# ── Paths ──────────────────────────────────────────────────────────────────────
ROOT = Path(__file__).parent.parent
DATA_DIR = Path(os.environ.get("RAILGUARD_DATA", str(ROOT / "data")))
RAILGUARD_BIN = ROOT / "build" / "railguard"
RAILGUARD_ENV = {**os.environ, "RAILGUARD_DATA": str(DATA_DIR)}


# ── CLI helper ─────────────────────────────────────────────────────────────────
def run_railguard(*args: str) -> str:
    """
    Invoke the railguard binary with the given arguments.
    Returns stdout on success; raises RuntimeError on failure or missing binary.
    """
    if not RAILGUARD_BIN.exists():
        raise RuntimeError(
            f"Binary not found: `{RAILGUARD_BIN}`\n\n"
            "Build the project first:\n"
            "```\ncmake -S .. -B ../build\ncmake --build ../build\n```"
        )
    result = subprocess.run(
        [str(RAILGUARD_BIN)] + list(args),
        capture_output=True,
        text=True,
        env=RAILGUARD_ENV,
        cwd=str(ROOT),
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "railguard returned non-zero exit code")
    return result.stdout


# ── CSV loaders ────────────────────────────────────────────────────────────────
@st.cache_data
def load_stations() -> pd.DataFrame:
    return pd.read_csv(DATA_DIR / "stations.csv")


@st.cache_data
def load_routes() -> pd.DataFrame:
    return pd.read_csv(DATA_DIR / "routes.csv")


@st.cache_data
def load_trains() -> pd.DataFrame:
    return pd.read_csv(DATA_DIR / "trains.csv")


# ── Passenger model (mirrors PassengerImpact.cpp constants) ───────────────────
def passengers_on_train(train_id: int, route: str) -> int:
    """
    Deterministic load formula matching PassengerModel in PassengerImpact.cpp:
        passengers = 650 + 45 * route_stops + 12 * train_id
    """
    stops = len(route.split(";")) if route else 1
    return 650 + 45 * stops + 12 * train_id


# ── Output parsers ─────────────────────────────────────────────────────────────
def parse_network_analysis(output: str) -> tuple[list[tuple[int, int]], list[int]]:
    """
    Parse `--analyze-network` stdout.

    Bridge lines look like: "  u v"   (two integers, leading spaces)
    AP line looks like:     "Articulation points: 0 (Name), 8 (Name), ..."
    """
    bridges: list[tuple[int, int]] = []
    aps: list[int] = []
    for line in output.splitlines():
        # Bridge: "  1 44"
        m = re.match(r"^\s{2}(\d+)\s+(\d+)\s*$", line)
        if m:
            bridges.append((int(m.group(1)), int(m.group(2))))
            continue
        # Articulation points line
        if line.startswith("Articulation points:"):
            # Format: "0 (Name), 8 (Name), ..."  — extract numeric ids before "("
            for num in re.findall(r"\b(\d+)\s+\(", line):
                aps.append(int(num))
    return bridges, aps


def parse_cascade(output: str) -> pd.DataFrame:
    """
    Parse `--cascade` stdout.
    Data lines: "  <train_id> <delay_min>"
    Returns DataFrame(train_id, delay_min).
    """
    rows = []
    for line in output.splitlines():
        m = re.match(r"^\s{2}(\d+)\s+(\d+)\s*$", line)
        if m:
            rows.append({"train_id": int(m.group(1)), "delay_min": int(m.group(2))})
    return pd.DataFrame(rows) if rows else pd.DataFrame(columns=["train_id", "delay_min"])


def parse_hotspots(output: str) -> pd.DataFrame:
    """
    Parse `--hotspots` stdout.
    Data lines: "<id> <name> fixed_mean=<f> var_max_mean=<f> var_best_len=<i> [HOT]"
    Station name may contain spaces; 'fixed_mean=' is the sentinel delimiter.
    Returns DataFrame(station_id, name, fixed_mean, var_max_mean, hot).
    """
    rows = []
    for line in output.splitlines():
        m = re.match(
            r"^(\d+)\s+(.+?)\s+fixed_mean=([\d.]+)\s+var_max_mean=([\d.]+)\s+var_best_len=(\d+)",
            line,
        )
        if m:
            rows.append(
                {
                    "station_id": int(m.group(1)),
                    "name": m.group(2).strip(),
                    "fixed_mean": float(m.group(3)),
                    "var_max_mean": float(m.group(4)),
                    "hot": line.rstrip().endswith("HOT"),
                }
            )
    cols = ["station_id", "name", "fixed_mean", "var_max_mean", "hot"]
    return pd.DataFrame(rows, columns=cols) if rows else pd.DataFrame(columns=cols)


def parse_intervene(output: str) -> dict:
    """
    Parse `--intervene` stdout.
    Expected lines:
      "Cascade from train N (M min seed): PDM before=X after K greedy clears=Y"
      "Chosen trains: a b c ..."
    Returns dict(pdm_before, pdm_after, chosen_trains).
    """
    result: dict = {"pdm_before": 0, "pdm_after": 0, "chosen_trains": []}
    for line in output.splitlines():
        m = re.search(r"PDM before=(\d+).*?clears=(\d+)", line)
        if m:
            result["pdm_before"] = int(m.group(1))
            result["pdm_after"] = int(m.group(2))
        if line.startswith("Chosen trains:"):
            nums = re.findall(r"\d+", line.split(":", 1)[1])
            result["chosen_trains"] = [int(x) for x in nums]
    return result


# ── Tab 1 — Network Map ────────────────────────────────────────────────────────
def tab_network_map() -> None:
    st.header("Network Map")
    st.caption(
        "Bridge edges — orange  |  Articulation-point stations — red  |  Normal stations — blue"
    )

    with st.spinner("Running Tarjan analysis…"):
        try:
            net_out = run_railguard("--analyze-network")
            bridges, aps = parse_network_analysis(net_out)
            error_msg = ""
        except RuntimeError as exc:
            bridges, aps = [], []
            error_msg = str(exc)

    if error_msg:
        st.error(error_msg)
        st.info("The map will render without bridge/AP highlights.")

    stations = load_stations()
    routes = load_routes()

    # Build graph using geographic coordinates as positions (lon=x, lat=y).
    G = nx.Graph()
    pos: dict[int, tuple[float, float]] = {}
    labels: dict[int, str] = {}
    for _, row in stations.iterrows():
        sid = int(row["station_id"])
        G.add_node(sid)
        pos[sid] = (float(row["lon"]), float(row["lat"]))
        labels[sid] = str(row["name"])

    bridge_set = {(min(u, v), max(u, v)) for u, v in bridges}
    normal_edges: list[tuple[int, int]] = []
    bridge_edges: list[tuple[int, int]] = []
    for _, row in routes.iterrows():
        u, v = int(row["from_id"]), int(row["to_id"])
        G.add_edge(u, v)
        key = (min(u, v), max(u, v))
        (bridge_edges if key in bridge_set else normal_edges).append((u, v))

    ap_set = set(aps)
    normal_nodes = [n for n in G.nodes() if n not in ap_set]
    ap_nodes = [n for n in G.nodes() if n in ap_set]

    fig, ax = plt.subplots(figsize=(16, 11))
    ax.set_facecolor("#f0f2f5")
    fig.patch.set_facecolor("#f0f2f5")

    nx.draw_networkx_edges(
        G, pos, edgelist=normal_edges, ax=ax,
        edge_color="#90a4ae", width=1.2, alpha=0.75,
    )
    nx.draw_networkx_edges(
        G, pos, edgelist=bridge_edges, ax=ax,
        edge_color="#fd7e14", width=3.0, alpha=0.95,
    )
    nx.draw_networkx_nodes(
        G, pos, nodelist=normal_nodes, ax=ax,
        node_color="#4c8fdd", node_size=130, alpha=0.92,
    )
    nx.draw_networkx_nodes(
        G, pos, nodelist=ap_nodes, ax=ax,
        node_color="#dc3545", node_size=260, alpha=0.98,
    )
    nx.draw_networkx_labels(G, pos, labels=labels, ax=ax, font_size=6, font_color="#1a1a2e")

    legend_handles = [
        mpatches.Patch(color="#4c8fdd", label="Station (normal)"),
        mpatches.Patch(color="#dc3545", label="Articulation point"),
        mpatches.Patch(color="#fd7e14", label="Bridge edge"),
        mpatches.Patch(color="#90a4ae", label="Normal edge"),
    ]
    ax.legend(handles=legend_handles, loc="lower left", fontsize=9, framealpha=0.85)
    ax.set_title("RailGuard — Indian Railways Network", fontsize=14, pad=14)
    ax.axis("off")
    plt.tight_layout()
    st.pyplot(fig)
    plt.close(fig)

    c1, c2, c3 = st.columns(3)
    c1.metric("Stations", len(stations))
    c2.metric("Bridge edges", len(bridges))
    c3.metric("Articulation points", len(aps))

    if aps:
        ap_name_map = stations.set_index("station_id")["name"].to_dict()
        st.write(
            "**Articulation points:**",
            " · ".join(f"{a} ({ap_name_map.get(a, '?')})" for a in sorted(aps)),
        )


# ── Tab 2 — Cascade Simulator ─────────────────────────────────────────────────
def tab_cascade() -> None:
    st.header("Cascade Simulator")
    trains = load_trains()

    options = {
        f"{int(r['train_id'])}: {r['name']}": int(r["train_id"])
        for _, r in trains.iterrows()
    }
    selected_label = st.selectbox("Select train", list(options.keys()), key="cas_train")
    selected_tid = options[selected_label]
    delay = st.slider("Initial delay (minutes)", min_value=10, max_value=120, value=30, step=5, key="cas_delay")

    if not st.button("Simulate Cascade", key="cas_btn"):
        st.caption("Select a train and delay, then press **Simulate Cascade**.")
        return

    with st.spinner("Running cascade propagation…"):
        try:
            out = run_railguard("--cascade", str(selected_tid), str(delay))
        except RuntimeError as exc:
            st.error(str(exc))
            return

    df = parse_cascade(out)
    if df.empty:
        st.warning("No affected trains returned for this cascade.")
        return

    # Merge train names and compute passenger impact.
    train_meta = trains.set_index("train_id")
    df["name"] = df["train_id"].map(train_meta["name"]).fillna("Unknown")
    df["route"] = df["train_id"].map(train_meta["route"]).fillna("")
    df["passengers"] = df.apply(
        lambda r: passengers_on_train(int(r["train_id"]), str(r["route"])), axis=1
    )
    df["pdm"] = df["delay_min"] * df["passengers"]
    df["label"] = df["train_id"].astype(str) + ": " + df["name"].str.slice(0, 16)

    # Bar chart — seed train highlighted in a deeper red.
    fig, ax = plt.subplots(figsize=(max(10, len(df) * 0.55), 5))
    colors = ["#c0392b" if tid == selected_tid else "#4c8fdd" for tid in df["train_id"]]
    ax.bar(df["label"], df["delay_min"], color=colors, edgecolor="white", linewidth=0.5)
    ax.axhline(delay, color="#fd7e14", linestyle="--", linewidth=1.5, label=f"Seed delay = {delay} min")
    ax.set_xlabel("Train", fontsize=10)
    ax.set_ylabel("Propagated delay (min)", fontsize=10)
    ax.set_title(
        f"Cascade from train {selected_tid} ({trains.set_index('train_id').loc[selected_tid, 'name']}) "
        f"— seed {delay} min",
        fontsize=11,
    )
    ax.legend(fontsize=9)
    plt.xticks(rotation=45, ha="right", fontsize=7)
    plt.tight_layout()
    st.pyplot(fig)
    plt.close(fig)

    total_pdm = int(df["pdm"].sum())
    total_pax = int(df["passengers"].sum())
    c1, c2, c3 = st.columns(3)
    c1.metric("Affected trains", len(df))
    c2.metric("Total passengers affected", f"{total_pax:,}")
    c3.metric("Total passenger-delay-minutes", f"{total_pdm:,}")

    with st.expander("Data table"):
        st.dataframe(
            df[["train_id", "name", "delay_min", "passengers", "pdm"]]
            .rename(columns={"delay_min": "delay (min)", "pdm": "PDM"})
            .reset_index(drop=True),
            use_container_width=True,
        )


# ── Tab 3 — Delay Hotspots ────────────────────────────────────────────────────
def tab_hotspots() -> None:
    st.header("Delay Hotspots")
    window = st.slider("Window size (days)", min_value=7, max_value=30, value=7, step=1, key="hs_win")
    threshold = st.number_input(
        "HOT threshold — mean delay (minutes)", value=35.0, step=5.0, key="hs_thr"
    )

    if not st.button("Compute Hotspots", key="hs_btn"):
        st.caption("Adjust the window, then press **Compute Hotspots**.")
        return

    with st.spinner("Analysing historical delay data…"):
        try:
            out = run_railguard("--hotspots", "--window", str(window), "--threshold", str(threshold))
        except RuntimeError as exc:
            st.error(str(exc))
            return

    df = parse_hotspots(out)
    if df.empty:
        st.warning("No hotspot data returned.")
        return

    # Bar chart — HOT stations in red, normal in blue.
    fig, ax = plt.subplots(figsize=(12, 5))
    colors = ["#dc3545" if h else "#4c8fdd" for h in df["hot"]]
    ax.bar(df["name"], df["fixed_mean"], color=colors, edgecolor="white", linewidth=0.5)
    ax.axhline(
        threshold, color="#fd7e14", linestyle="--", linewidth=1.8,
        label=f"HOT threshold = {threshold:.0f} min",
    )
    ax.set_xlabel("Station", fontsize=10)
    ax.set_ylabel(f"Mean daily delay — last {window} days (min)", fontsize=10)
    ax.set_title(f"Top 10 Delay Hotspots — {window}-day sliding window", fontsize=12)
    ax.legend(fontsize=9)

    legend_handles = [
        mpatches.Patch(color="#dc3545", label="HOT station"),
        mpatches.Patch(color="#4c8fdd", label="Normal station"),
    ]
    ax.legend(handles=legend_handles + [
        plt.Line2D([0], [0], color="#fd7e14", linestyle="--", linewidth=1.8,
                   label=f"Threshold = {threshold:.0f} min")
    ], fontsize=9)

    plt.xticks(rotation=40, ha="right", fontsize=8)
    plt.tight_layout()
    st.pyplot(fig)
    plt.close(fig)

    hot_count = int(df["hot"].sum())
    avg_mean = float(df["fixed_mean"].mean())
    worst_station = df.loc[df["fixed_mean"].idxmax(), "name"]
    c1, c2, c3 = st.columns(3)
    c1.metric("HOT stations (top 10)", hot_count)
    c2.metric("Avg mean delay (top 10)", f"{avg_mean:.1f} min")
    c3.metric("Worst station", worst_station)

    with st.expander("Data table"):
        st.dataframe(
            df[["station_id", "name", "fixed_mean", "var_max_mean", "hot"]]
            .rename(columns={
                "fixed_mean": f"{window}-day mean (min)",
                "var_max_mean": "var-window max mean",
                "hot": "HOT",
            })
            .reset_index(drop=True),
            use_container_width=True,
        )


# ── Tab 4 — Intervention Planner ──────────────────────────────────────────────
def tab_intervention() -> None:
    st.header("Intervention Planner")
    trains = load_trains()

    options = {
        f"{int(r['train_id'])}: {r['name']}": int(r["train_id"])
        for _, r in trains.iterrows()
    }
    selected_label = st.selectbox("Select seed train", list(options.keys()), key="int_train")
    selected_tid = options[selected_label]

    c1, c2 = st.columns(2)
    budget = c1.slider("Budget K (interventions)", min_value=1, max_value=10, value=3, key="int_k")
    seed_delay = c2.slider("Seed delay (minutes)", min_value=10, max_value=120, value=60, step=5, key="int_delay")

    if not st.button("Run Optimizer", key="int_btn"):
        st.caption("Configure the cascade, then press **Run Optimizer**.")
        return

    with st.spinner("Optimising interventions…"):
        try:
            out = run_railguard(
                "--intervene", str(selected_tid),
                "--budget", str(budget),
                "--seed-delay", str(seed_delay),
            )
        except RuntimeError as exc:
            st.error(str(exc))
            return

    data = parse_intervene(out)
    pdm_before = data["pdm_before"]
    pdm_after = data["pdm_after"]
    chosen = data["chosen_trains"]

    if pdm_before == 0:
        st.warning("PDM before = 0. Try a higher seed delay or a different train.")
        return

    reduction_pct = (pdm_before - pdm_after) / pdm_before * 100

    # Big metric at top.
    st.metric(
        label="PDM reduction",
        value=f"{reduction_pct:.1f}%",
        delta=f"-{pdm_before - pdm_after:,} passenger-delay-minutes",
        delta_color="inverse",
    )

    # Before / after comparison bar.
    fig, ax = plt.subplots(figsize=(6, 4))
    bar_labels = ["Before interventions", f"After {len(chosen)} intervention(s)"]
    bar_vals = [pdm_before, pdm_after]
    bar_colors = ["#dc3545", "#28a745"]
    bars = ax.bar(bar_labels, bar_vals, color=bar_colors, edgecolor="white", width=0.45)
    for bar, val in zip(bars, bar_vals):
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + pdm_before * 0.012,
            f"{val:,}",
            ha="center", va="bottom", fontsize=11, fontweight="bold",
        )
    ax.set_ylabel("Passenger-delay-minutes (PDM)", fontsize=10)
    ax.set_title(
        f"Train {selected_tid} · seed {seed_delay} min · budget K={budget}",
        fontsize=11,
    )
    ax.set_ylim(0, pdm_before * 1.18)
    plt.tight_layout()
    st.pyplot(fig)
    plt.close(fig)

    c1, c2, c3 = st.columns(3)
    c1.metric("PDM before", f"{pdm_before:,}")
    c2.metric("PDM after", f"{pdm_after:,}")
    c3.metric("Interventions used", len(chosen))

    st.subheader("Trains selected for intervention (greedy order)")
    if chosen:
        train_name_map = trains.set_index("train_id")["name"].to_dict()
        rows = [{"train_id": t, "name": train_name_map.get(t, "Unknown")} for t in chosen]
        st.dataframe(pd.DataFrame(rows).reset_index(drop=True), use_container_width=True)
    else:
        st.info("No trains were selected (cascade may be trivially empty).")


# ── App shell ──────────────────────────────────────────────────────────────────
def main() -> None:
    st.set_page_config(
        page_title="RailGuard Dashboard",
        page_icon=":railway_car:",
        layout="wide",
    )
    st.title("RailGuard — Indian Railways Delay Analytics")
    st.caption(
        f"Data: `{DATA_DIR}` · Binary: `{RAILGUARD_BIN}` · "
        "Override data path with `RAILGUARD_DATA` env var."
    )

    t1, t2, t3, t4 = st.tabs([
        "Network Map",
        "Cascade Simulator",
        "Delay Hotspots",
        "Intervention Planner",
    ])
    with t1:
        tab_network_map()
    with t2:
        tab_cascade()
    with t3:
        tab_hotspots()
    with t4:
        tab_intervention()


if __name__ == "__main__":
    main()
