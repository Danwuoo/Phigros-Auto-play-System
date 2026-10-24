"""Offline diagnostic: original video frame plus C++ line poses, never timing gold.

Requires task-local PyAV and Pillow. The alignment is a supplied measured coarse
WAV sample offset. Audio-frame PTS mapping preserves gaps in the recording.
"""
import argparse
import bisect
import hashlib
import json
import math
import pathlib
import subprocess


def sha(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--video", type=pathlib.Path, required=True)
    parser.add_argument("--audio-index", type=pathlib.Path, required=True)
    parser.add_argument("--chart", type=pathlib.Path, required=True)
    parser.add_argument("--chart-sha", required=True)
    parser.add_argument("--pose-exe", type=pathlib.Path, required=True)
    parser.add_argument("--wav-song-offset", type=float, required=True)
    parser.add_argument("--pose-phase-seconds", type=float, default=0,
                        help="explicit visual-chart phase hypothesis; does not alter selected video frames")
    parser.add_argument("--song-seconds", type=float, nargs="+", required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args()
    if not math.isfinite(args.wav_song_offset) or not math.isfinite(args.pose_phase_seconds) or abs(args.pose_phase_seconds)>5 or any(not math.isfinite(t) or not 0 <= t <= 3600 for t in args.song_seconds):
        parser.error("invalid finite time")
    if len(args.song_seconds) > 24 or args.audio_index.stat().st_size > 8 * 1024 * 1024:
        parser.error("diagnostic input budget")
    import av
    from PIL import Image, ImageDraw
    rows = [json.loads(line) for line in args.audio_index.read_text("utf-8").splitlines()]
    if not rows or any(r["sample_rate"] != rows[0]["sample_rate"] for r in rows):
        raise ValueError("missing or changing audio sample rate")
    rate = rows[0]["sample_rate"]
    samples = [r["sample_frame_offset"] for r in rows]
    pts = [r["pts_seconds"] for r in rows]
    if any(b <= a for a, b in zip(pts, pts[1:])):
        raise ValueError("audio PTS must increase")
    def wav_to_pts(seconds):
        sample = seconds * rate
        i = max(0, bisect.bisect_right(samples, sample) - 1)
        row = rows[i]
        if not 0 <= sample - samples[i] <= row["sample_frames"]:
            raise ValueError("requested sample outside recording")
        return pts[i] + (sample - samples[i]) / rate
    def pts_to_wav(seconds):
        i = max(0, bisect.bisect_right(pts, seconds) - 1)
        row = rows[i]
        within = seconds - pts[i]
        if not 0 <= within <= row["sample_frames"] / rate:
            raise ValueError("selected video frame is in an audio PTS gap")
        return samples[i] / rate + within
    requested = [wav_to_pts(t + args.wav_song_offset) for t in args.song_seconds]
    best = [None for _ in requested]
    # A single bounded decoder pass avoids repeated seek and codec-preroll ambiguity.
    with av.open(str(args.video)) as container:
        for ordinal, frame in enumerate(container.decode(video=0)):
            if ordinal > 120000:
                raise ValueError("frame budget")
            if frame.pts is None:
                continue
            seconds = float(frame.pts * frame.time_base)
            for i, target in enumerate(requested):
                distance = abs(seconds - target)
                if best[i] is None or distance < best[i][0]:
                    best[i] = (distance, seconds, ordinal, frame.to_image())
            if seconds > max(requested) + 1:
                break
    args.out.mkdir(parents=False, exist_ok=False)
    manifest = {"schema": "pas.pose-video-overlay.v1", "video_sha256": sha(args.video),
                "audio_index_sha256": sha(args.audio_index), "chart_sha256": args.chart_sha,
                "pose_executable_sha256": sha(args.pose_exe), "wav_song_offset_seconds": args.wav_song_offset,
                "visual_pose_phase_hypothesis_seconds": args.pose_phase_seconds,
                "scope": "Coarse audio-aligned visual geometry check; not judgment or millisecond timing gold.",
                "clock_mapping": "WAV concatenated sample -> recorded audio packet PTS -> nearest video PTS",
                "preview_policy": "cyan line at frame-inferred song time; magenta target near judgment or during Hold",
                "frames": []}
    thumbnails = []
    for i, sample in enumerate(best):
        if sample is None:
            raise ValueError("no decoded video frame")
        distance, seconds, ordinal, image = sample
        song = pts_to_wav(seconds) - args.wav_song_offset + args.pose_phase_seconds
        result = subprocess.run([str(args.pose_exe), str(args.chart), args.chart_sha, str(round(song * 1e9))],
                                check=True, capture_output=True, text=True, timeout=30)
        pose = json.loads(result.stdout)
        image.save(args.out / f"{i:02d}-original.png")
        overlay = image.copy()
        draw = ImageDraw.Draw(overlay)
        width, height = overlay.size
        for line in pose["lines"]:
            x, y = line["origin"]; tx, ty = line["tangent"]
            if line["opacity"] <= .05 or not (-2 < x < 3 and -2 < y < 3):
                continue
            # Tangent defined in canonical isotropic pixels, scale to this frame.
            dx, dy = tx * 1800 * width / 1280, ty * 1800 * height / 720
            draw.line((x*width-dx,y*height-dy,x*width+dx,y*height+dy), fill=(0,235,255), width=2)
            if 0 <= x <= 1 and 0 <= y <= 1:
                draw.text((x*width+8,y*height+8),f"L{line['line']}", fill=(0,235,255))
        for note in pose["nearby_notes"]:
            if not note["inside_reference"]:
                continue
            x, y = note["target"]; x *= width; y *= height
            draw.ellipse((x-9,y-9,x+9,y+9),outline=(255,50,230),width=3)
        draw.rectangle((0,0,width,54),fill=(8,12,20))
        draw.text((12,10),f"COARSE GEOMETRY ONLY | song ~{song:.3f}s | video PTS {seconds:.3f}s | frame {ordinal}",fill="white")
        draw.text((12,30),"cyan: computed line | magenta: line-center contact target | original paired separately",fill=(190,220,230))
        overlay.save(args.out / f"{i:02d}-overlay.png")
        (args.out / f"{i:02d}-pose.json").write_text(json.dumps(pose,ensure_ascii=False,indent=2)+"\n","utf-8")
        thumbnail = overlay.copy(); thumbnail.thumbnail((640,360)); thumbnails.append(thumbnail)
        manifest["frames"].append({"requested_song_seconds":args.song_seconds[i],"inferred_song_seconds":song,
            "frame_ordinal":ordinal,"frame_pts_seconds":seconds,"nearest_frame_delta_seconds":distance,
            "original":f"{i:02d}-original.png","overlay":f"{i:02d}-overlay.png","pose":f"{i:02d}-pose.json"})
    sheet = Image.new("RGB",(1280,360*((len(thumbnails)+1)//2)),(8,12,20))
    for i, thumb in enumerate(thumbnails):
        sheet.paste(thumb,((i%2)*640,(i//2)*360))
    sheet.save(args.out / "overview.png")
    (args.out/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n","utf-8")
    print(json.dumps({"out":str(args.out),"frames":len(best)}))

if __name__ == "__main__":
    main()
