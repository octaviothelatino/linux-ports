#!/usr/bin/env python3
"""Measure desktop composition in a recorded game session.

The analyzer samples the decoded video at a known rate, measures changes in
the full frame and optional regions, and writes contact sheets plus selected
frames. It does not infer success from process logs or browser navigation.
"""

import argparse
import json
import math
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFont, ImageStat


def run_probe(video):
    command = [
        "ffprobe", "-v", "error", "-select_streams", "v:0",
        "-show_entries", "stream=width,height,duration,avg_frame_rate,nb_frames",
        "-of", "json", str(video),
    ]
    result = subprocess.run(command, check=True, capture_output=True, text=True)
    stream = json.loads(result.stdout)["streams"][0]
    width = int(stream["width"])
    height = int(stream["height"])
    duration = float(stream.get("duration") or 0.0)
    return stream, width, height, duration


def parse_region(value, width, height):
    if value is None:
        return None
    try:
        x, y, w, h = (int(part) for part in value.split(","))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("region must be x,y,width,height") from exc
    if w <= 0 or h <= 0 or x < 0 or y < 0 or x + w > width or y + h > height:
        raise argparse.ArgumentTypeError("region must fit inside the video")
    return x, y, w, h


def crop(image, region):
    if region is None:
        return image
    x, y, w, h = region
    return image.crop((x, y, x + w, y + h))


def gray_small(image):
    return image.convert("L").resize((max(1, image.width // 8), max(1, image.height // 8)))


def difference(previous, current):
    if previous is None:
        return 0.0
    delta = ImageChops.difference(gray_small(previous), gray_small(current))
    return ImageStat.Stat(delta).mean[0]


def annotate(image, timestamp, full_diff, region_diffs):
    image = image.copy()
    draw = ImageDraw.Draw(image)
    lines = [f"t={timestamp:.3f}s full-change={full_diff:.2f}"]
    lines.extend(f"{name}-change={value:.2f}" for name, value in region_diffs.items())
    box_height = 20 * len(lines) + 8
    draw.rectangle((0, 0, min(image.width, 440), box_height), fill=(0, 0, 0))
    for index, line in enumerate(lines):
        draw.text((5, 4 + index * 20), line, fill=(255, 255, 255))
    return image


def make_sheet(images, output, columns):
    if not images:
        return
    thumb_width = 320
    thumb_height = round(images[0].height * thumb_width / images[0].width)
    rows = math.ceil(len(images) / columns)
    sheet = Image.new("RGB", (columns * thumb_width, rows * thumb_height), "black")
    for index, image in enumerate(images):
        thumb = image.copy()
        thumb.thumbnail((thumb_width, thumb_height), Image.Resampling.LANCZOS)
        x = (index % columns) * thumb_width
        y = (index // columns) * thumb_height
        sheet.paste(thumb, (x, y))
    sheet.save(output)


def read_frame(stream, frame_size):
    chunks = []
    remaining = frame_size
    while remaining:
        chunk = stream.read(remaining)
        if not chunk:
            return None
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def prepare_output(output):
    output = output.resolve()
    marker = output / ".footage-analysis-managed"
    output.mkdir(parents=True, exist_ok=True)
    existing = list(output.iterdir())
    if marker not in existing and existing:
        raise RuntimeError(
            f"refusing to clear non-analyzer directory {output}; "
            "choose an empty output directory"
        )
    for child in existing:
        if child == marker:
            continue
        if child.is_dir():
            shutil.rmtree(child)
        else:
            child.unlink()
    marker.write_text("This directory is managed by analyze-footage.py.\n")
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("video", type=Path)
    parser.add_argument(
        "--output", type=Path, default=Path("test-results/footage-latest")
    )
    parser.add_argument("--sample-fps", type=float, default=60.0)
    parser.add_argument("--preview-fps", type=float, default=2.0)
    parser.add_argument("--panel", help="HTML panel region as x,y,width,height")
    parser.add_argument("--game", help="game window region as x,y,width,height")
    parser.add_argument("--change-threshold", type=float, default=1.0)
    args = parser.parse_args()

    stream, width, height, duration = run_probe(args.video)
    panel = parse_region(args.panel, width, height)
    game = parse_region(args.game, width, height)
    regions = {name: region for name, region in (("panel", panel), ("game", game)) if region}
    args.output = prepare_output(args.output)
    selected_dir = args.output / "selected"
    selected_dir.mkdir(exist_ok=True)

    metadata = {
        "video": str(args.video),
        "width": width,
        "height": height,
        "duration_seconds": duration,
        "sample_fps": args.sample_fps,
        "preview_fps": args.preview_fps,
        "panel_region": panel,
        "game_region": game,
        "change_threshold": args.change_threshold,
        "stream": stream,
    }
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")

    filter_args = f"fps={args.sample_fps:g}"
    command = [
        "ffmpeg", "-v", "error", "-i", str(args.video), "-vf", filter_args,
        "-f", "rawvideo", "-pix_fmt", "rgb24", "-",
    ]
    frame_size = width * height * 3
    process = subprocess.Popen(command, stdout=subprocess.PIPE)
    previous = None
    preview_images = []
    selected_images = []
    measurements = []
    frame_index = 0
    preview_step = max(1, round(args.sample_fps / args.preview_fps))

    try:
        while True:
            raw = read_frame(process.stdout, frame_size)
            if raw is None:
                break
            image = Image.frombytes("RGB", (width, height), raw)
            timestamp = frame_index / args.sample_fps
            full_diff = difference(previous, image)
            region_diffs = {
                name: difference(crop(previous, region) if previous else None,
                                 crop(image, region))
                for name, region in regions.items()
            }
            measurements.append({
                "frame": frame_index,
                "time_seconds": round(timestamp, 6),
                "full_change": round(full_diff, 6),
                **{f"{name}_change": round(value, 6)
                   for name, value in region_diffs.items()},
            })
            if frame_index % preview_step == 0:
                preview_images.append(annotate(image, timestamp, full_diff, region_diffs))
            if full_diff >= args.change_threshold:
                selected = annotate(image, timestamp, full_diff, region_diffs)
                selected.save(selected_dir / f"frame-{frame_index:06d}.png")
                if len(selected_images) < 40:
                    selected_images.append(selected)
            previous = image
            frame_index += 1
    finally:
        process.stdout.close()
        return_code = process.wait()
    if return_code:
        raise RuntimeError(f"ffmpeg exited with status {return_code}")

    make_sheet(preview_images, args.output / "preview-contact-sheet.png", 5)
    make_sheet(selected_images[:40], args.output / "change-contact-sheet.png", 5)
    (args.output / "measurements.json").write_text(json.dumps(measurements, indent=2) + "\n")

    changes = [item["full_change"] for item in measurements[1:]]
    if changes:
        largest = sorted(measurements[1:], key=lambda item: item["full_change"], reverse=True)[:10]
    else:
        largest = []
    report = [
        f"frames={len(measurements)}",
        f"duration_sampled={len(measurements) / args.sample_fps:.3f}s",
        f"change_frames={len(selected_images)}",
        f"largest_full_changes={json.dumps(largest)}",
        "",
        "Interpretation guide:",
        "full change high and game change high means the game area changed.",
        "full change high and panel change high while game change stays low means panel-only change.",
        "A stable map with no panel change does not prove that the panel is visible. Use the screen recording and window evidence together.",
    ]
    (args.output / "report.txt").write_text("\n".join(report) + "\n")
    print("\n".join(report))


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"analyze-footage: {exc}", file=sys.stderr)
        sys.exit(1)
