#!/usr/bin/env python3
"""Rate a cached top-surface projection with interpretable diagnostics, not an art score.

Dependencies: NumPy, Matplotlib. Units and thresholds are metres/degrees.
Prominence is measured inside this finite window; caves and mesh LOD are not rated.
"""

import argparse
import csv
import hashlib
import json
import math
import os
import tempfile
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "worldgen-matplotlib"))
import matplotlib  # noqa: E402

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402


def load_surface(path):
    with path.open() as file:
        metadata = json.loads(file.readline().removeprefix("# "))
        rows = list(csv.DictReader(file))
    xs = np.unique([float(row["x"]) for row in rows])
    zs = np.unique([float(row["z"]) for row in rows])
    if len(xs) < 3 or len(zs) < 3:
        raise ValueError("Need at least a 3 by 3 height map")
    heights = np.full((len(zs), len(xs)), np.nan)
    status = {}
    for row in rows:
        status[row["status"]] = status.get(row["status"], 0) + 1
        if row["status"] == "valid":
            heights[np.searchsorted(zs, float(row["z"])), np.searchsorted(xs, float(row["x"]))] = float(row["height"])
    if not np.isfinite(heights).any():
        raise ValueError("No valid top surface: inspect clipped_top/no_surface CSV statuses")
    dx, dz = float(xs[1] - xs[0]), float(zs[1] - zs[0])
    if not np.allclose(np.diff(xs), dx) or not np.allclose(np.diff(zs), dz):
        raise ValueError("Nonuniform diagnostic lattice")
    manifest_path = Path(str(path) + ".json")
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else None
    return {
        "path": str(path),
        "metadata": metadata,
        "manifest": manifest,
        "status": status,
        "x": xs,
        "z": zs,
        "h": heights,
        "dx": dx,
        "dz": dz,
    }


def gaussian(surface, sigma_m):
    valid = np.isfinite(surface["h"])
    values = np.where(valid, surface["h"], 0.0)
    weights = valid.astype(float)
    for axis, spacing in [(1, surface["dx"]), (0, surface["dz"])]:
        sigma = sigma_m / spacing
        radius = max(1, math.ceil(sigma * 3))
        offsets = np.arange(-radius, radius + 1)
        kernel = np.exp(-0.5 * (offsets / sigma) ** 2)
        kernel /= kernel.sum()

        def convolve(array):
            padding = [(0, 0), (0, 0)]
            padding[axis] = (radius, radius)
            padded = np.pad(array, padding, mode="edge")
            return np.apply_along_axis(lambda row: np.convolve(row, kernel, mode="valid"), axis, padded)

        values, weights = convolve(values), convolve(weights)
    result = np.divide(values, weights, out=np.full_like(values, np.nan), where=weights > 1e-12)
    result[~valid] = np.nan
    return result


def prominences(heights):
    """Descending 8-connected union/find; merge levels give finite-window saddles."""
    ny, nx = heights.shape
    flat = heights.ravel()
    valid = np.isfinite(flat)
    order = np.flatnonzero(valid)
    order = order[np.argsort(-flat[order], kind="stable")]
    parents = np.full(flat.size, -1, dtype=np.int64)
    summits = np.full(flat.size, -1, dtype=np.int64)
    touches_edge = np.zeros(flat.size, dtype=bool)
    peaks = []

    def find(index):
        while parents[index] != index:
            parents[index] = parents[parents[index]]
            index = int(parents[index])
        return index

    for value in order:
        index = int(value)
        z, x = divmod(index, nx)
        parents[index] = index
        summits[index] = index
        touches_edge[index] = x in (0, nx - 1) or z in (0, ny - 1)
        for dz in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if not (dx or dz):
                    continue
                xx, zz = x + dx, z + dz
                if not (0 <= xx < nx and 0 <= zz < ny):
                    continue
                neighbor = xx + zz * nx
                if not valid[neighbor]:
                    touches_edge[find(index)] = True
                if parents[neighbor] < 0:
                    continue
                a, b = find(index), find(neighbor)
                if a == b:
                    continue
                pa, pb = int(summits[a]), int(summits[b])
                if (flat[pa], -pa) < (flat[pb], -pb):
                    a, b, pa, pb = b, a, pb, pa
                prominence = float(flat[pb] - flat[index])
                if prominence > 1e-5:
                    peaks.append({
                        "index": pb,
                        "height_m": float(flat[pb]),
                        "prominence_m": prominence,
                        "edge_censored": bool(touches_edge[b]),
                    })
                parents[b] = a
                touches_edge[a] |= touches_edge[b]
    for root in {find(int(i)) for i in order}:
        peak = int(summits[root])
        peaks.append({
            "index": peak,
            "height_m": float(flat[peak]),
            "prominence_m": float(flat[peak] - np.nanmin(flat)),
            "edge_censored": True,
        })
    return peaks


def sample_bilinear(heights, x, z):
    ix, iz = int(math.floor(x)), int(math.floor(z))
    if ix < 0 or iz < 0 or ix + 1 >= heights.shape[1] or iz + 1 >= heights.shape[0]:
        return math.nan
    q = heights[iz : iz + 2, ix : ix + 2]
    if not np.isfinite(q).all():
        return math.nan
    fx, fz = x - ix, z - iz
    return float((q[0, 0] * (1 - fx) + q[0, 1] * fx) * (1 - fz) + (q[1, 0] * (1 - fx) + q[1, 1] * fx) * fz)


def analyze(surface, prominence_m, mountain_height_m):
    heights = surface["h"]
    dx, dz = surface["dx"], surface["dz"]
    dz_values, dx_values = np.gradient(heights, dz, dx)
    slopes = np.degrees(np.arctan(np.hypot(dx_values, dz_values)))
    broad = gaussian(surface, 2.0)
    residual = heights - broad
    peaks = []
    for peak in prominences(heights):
        z, x = divmod(peak.pop("index"), heights.shape[1])
        margin = min(x * dx, z * dz, (heights.shape[1] - 1 - x) * dx, (heights.shape[0] - 1 - z) * dz)
        if peak["prominence_m"] < prominence_m or margin < 8:
            continue
        ring = [
            sample_bilinear(heights, x + math.cos(a) * 2 / dx, z + math.sin(a) * 2 / dz)
            for a in np.linspace(0, 2 * math.pi, 16, endpoint=False)
        ]
        valid_ring = [value for value in ring if math.isfinite(value)]
        if not valid_ring:
            continue
        drop = peak["height_m"] - float(np.mean(valid_ring))
        peak.update(x_m=float(surface["x"][x]), z_m=float(surface["z"][z]), apex_drop_2m=drop, narrow_tip=drop >= 1.0)
        peaks.append(peak)
    mountain = [p for p in peaks if p["height_m"] >= mountain_height_m]
    peak_heights = np.array([p["height_m"] for p in mountain])
    drops = np.array([p["apex_drop_2m"] for p in mountain])

    def percentile(values, percent):
        return float(np.percentile(values, percent)) if len(values) else None

    metrics = {
        "analysis_tool_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "source": surface["path"],
        "source_sha256": hashlib.sha256(Path(surface["path"]).read_bytes()).hexdigest(),
        "metadata": surface["metadata"],
        "manifest": surface["manifest"],
        "statuses": surface["status"],
        "spacing_m": [dx, dz],
        "shape": list(heights.shape),
        "apex_metric_resolution_limited": max(dx, dz) > 1,
        "thresholds": {
            "prominence_m": prominence_m,
            "mountain_height_m": mountain_height_m,
            "edge_margin_m": 8,
            "apex_ring_m": 2,
            "narrow_tip_drop_m": 1,
            "smoothing_sigma_m": 2,
        },
        "valid_fraction": float(np.isfinite(heights).mean()),
        "height_min_m": float(np.nanmin(heights)),
        "height_max_m": float(np.nanmax(heights)),
        "height_p95_minus_p5_m": float(np.nanpercentile(heights, 95) - np.nanpercentile(heights, 5)),
        "slope_p95_degrees": float(np.nanpercentile(slopes, 95)) if np.isfinite(slopes).any() else None,
        "steep_area_fraction_over_65_degrees": float(np.mean(slopes[np.isfinite(slopes)] > 65))
        if np.isfinite(slopes).any()
        else None,
        "positive_highpass_p99_m": float(np.nanpercentile(np.maximum(residual, 0), 99)),
        "prominent_peaks": len(peaks),
        "mountain_peaks": len(mountain),
        "mountain_peak_heights_m": sorted(peak_heights.tolist()),
        "mountain_peak_height_span_m": float(np.ptp(peak_heights)) if len(peak_heights) else None,
        "mountain_peak_height_cv": float(peak_heights.std() / abs(peak_heights.mean()))
        if len(peak_heights) > 1 and abs(peak_heights.mean()) > 1e-6
        else None,
        "mountain_apex_drop_p90_m": percentile(drops, 90),
        "mountain_narrow_tips": sum(p["narrow_tip"] for p in mountain),
        "edge_censored_selected_peaks": sum(p["edge_censored"] for p in peaks),
        "limitations": [
            "Diagnostic top surface of the cached field before mesh LOD; hidden caves/overhangs and renderer artifacts are not represented.",
            "Prominence is finite-window; censored peaks are flagged. Thresholds are heuristics, not a universal visual quality score.",
            "Flat terrain can score low on sharpness: inspect retained relief, height variation and screenshots together.",
        ],
    }
    return {"surface": surface, "metrics": metrics, "peaks": peaks, "slope": slopes, "residual": residual}


def panel(ax, result, vmin, vmax, title):
    surface = result["surface"]
    extent = [surface["x"][0], surface["x"][-1], surface["z"][0], surface["z"][-1]]
    image = ax.imshow(surface["h"], origin="lower", extent=extent, vmin=vmin, vmax=vmax, cmap="terrain")
    if vmax - vmin > 1:
        ax.contour(
            surface["x"],
            surface["z"],
            surface["h"],
            levels=np.arange(math.floor(vmin / 5) * 5, vmax + 5, 5),
            colors="black",
            linewidths=0.35,
            alpha=0.35,
        )
    for peak in result["peaks"]:
        if peak["height_m"] >= result["metrics"]["thresholds"]["mountain_height_m"]:
            ax.plot(peak["x_m"], peak["z_m"], "^", color="red" if peak["narrow_tip"] else "black", markersize=4)
    ax.set(title=title, xlabel="World X (m)", ylabel="World Z (m)")
    return image


def save(result, destination):
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "metrics.json").write_text(json.dumps(result["metrics"], indent=2, allow_nan=False) + "\n")
    if result["peaks"]:
        with (destination / "peaks.csv").open("w") as file:
            writer = csv.DictWriter(file, fieldnames=list(result["peaks"][0]))
            writer.writeheader()
            writer.writerows(result["peaks"])
    fig, axes = plt.subplots(1, 3, figsize=(16, 5), constrained_layout=True)
    h = result["surface"]["h"]
    image = panel(
        axes[0], result, float(np.nanmin(h)), float(np.nanmax(h)), "Height / 5 m contours / prominent summits"
    )
    fig.colorbar(image, ax=axes[0], label="Metres")
    extent = [
        result["surface"]["x"][0],
        result["surface"]["x"][-1],
        result["surface"]["z"][0],
        result["surface"]["z"][-1],
    ]
    image = axes[1].imshow(result["slope"], origin="lower", extent=extent, cmap="magma", vmin=0, vmax=80)
    axes[1].set_title("Slope (degrees)")
    fig.colorbar(image, ax=axes[1])
    image = axes[2].imshow(result["residual"], origin="lower", extent=extent, cmap="RdBu_r", vmin=-2, vmax=2)
    axes[2].set_title("Height minus 2 m-smoothed surface (m)")
    fig.colorbar(image, ax=axes[2])
    fig.savefig(destination / "heightmap.png", dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--prominence", type=float, default=2)
    parser.add_argument("--mountain-height", type=float, default=8)
    args = parser.parse_args()
    if not math.isfinite(args.prominence) or args.prominence <= 0:
        parser.error("--prominence must be finite and positive")
    if not math.isfinite(args.mountain_height):
        parser.error("--mountain-height must be finite")
    result = analyze(load_surface(args.csv), args.prominence, args.mountain_height)
    save(result, args.output)
    if args.baseline:
        before = analyze(load_surface(args.baseline), args.prominence, args.mountain_height)
        a, b = before["surface"], result["surface"]
        if not np.array_equal(a["x"], b["x"]) or not np.array_equal(a["z"], b["z"]):
            raise ValueError("Before/after comparisons require identical coordinates")
        if a["manifest"] and b["manifest"] and a["manifest"].get("layout_seed") != b["manifest"].get("layout_seed"):
            raise ValueError("Paired comparisons require the same layout seed")
        save(before, args.output / "baseline")
        comparison = {"before": before["metrics"], "after": result["metrics"]}
        (args.output / "comparison.json").write_text(json.dumps(comparison, indent=2, allow_nan=False) + "\n")
        lines = [
            "# Terrain comparison",
            "",
            "Metrics describe this finite window, not a universal visual quality score.",
            "",
            "| Metric | Before | After |",
            "| --- | ---: | ---: |",
        ]
        labels = [
            ("mountain_peaks", "Prominent high-terrain peaks", "count"),
            ("mountain_narrow_tips", "Flagged narrow tips", "count"),
            ("mountain_peak_height_cv", "Relative peak-height variation (std/mean)", "percent"),
            ("mountain_peak_height_span_m", "Peak-height span (m)", "metres"),
            ("mountain_apex_drop_p90_m", "90th-percentile drop on a 2 m apex ring (m)", "metres"),
            ("positive_highpass_p99_m", "99th-percentile positive small-scale relief (m)", "metres"),
            ("height_p95_minus_p5_m", "Overall 5th–95th percentile relief (m)", "metres"),
            ("height_max_m", "Maximum surface height (m)", "metres"),
            ("steep_area_fraction_over_65_degrees", "Surface steeper than 65 degrees", "percent"),
        ]
        for key, label, units in labels:

            def display(value):
                if value is None:
                    return "n/a"
                if units == "percent":
                    return f"{100 * value:.1f}%"
                return str(int(value)) if units == "count" else f"{value:.2f}"

            lines.append(f"| {label} | {display(before['metrics'][key])} | {display(result['metrics'][key])} |")
        lines.extend([
            "",
            "Red map markers indicate prominent high-terrain summits with at least 1 m mean drop on a 2 m ring.",
            "Prominence is window-limited; see metrics.json for sampling, censoring and thresholds.",
            "",
        ])
        (args.output / "comparison.md").write_text("\n".join(lines))
        fig, axes = plt.subplots(1, 2, figsize=(13, 6), constrained_layout=True)
        vmin = min(np.nanmin(a["h"]), np.nanmin(b["h"]))
        vmax = max(np.nanmax(a["h"]), np.nanmax(b["h"]))
        panel(axes[0], before, vmin, vmax, "Before — common height scale")
        image = panel(axes[1], result, vmin, vmax, "After — red markers: narrow summit tips")
        fig.colorbar(image, ax=axes, label="Surface elevation (m)")
        fig.savefig(args.output / "comparison.png", dpi=160)
        plt.close(fig)
    print(json.dumps(result["metrics"], indent=2, allow_nan=False))


if __name__ == "__main__":
    main()
