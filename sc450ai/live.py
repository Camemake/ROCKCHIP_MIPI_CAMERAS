"""Live MJPEG of the SC450AI on the Luckfox Aura, http://0.0.0.0:8090/."""

import ctypes
import os
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

TJPF_RGB = 0
TJSAMP_420 = 2
_tj = ctypes.CDLL("/oem/usr/lib/libturbojpeg.so")
_tj.tjInitCompress.restype = ctypes.c_void_p
_tj.tjCompress2.argtypes = [
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_void_p),
    ctypes.POINTER(ctypes.c_ulong),
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
]
_tj.tjCompress2.restype = ctypes.c_int
_tj.tjFree.argtypes = [ctypes.c_void_p]
_JPEG = _tj.tjInitCompress()

WIDTH = 2688
HEIGHT = 1520
OUT_W = WIDTH // 2
OUT_H = HEIGHT // 2
OUT_BYTES = OUT_W * OUT_H * 3
PORT = 8090
HERE = os.path.dirname(os.path.abspath(__file__))

MEDIA = None
VIDEO = None
latest = b""
lock = threading.Lock()
fps = 0.0


def sh(*args):
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(" ".join(args) + "\n" + result.stdout + result.stderr)
    return result.stdout


def find_nodes():
    global MEDIA, VIDEO
    for index in range(16):
        path = "/dev/media%d" % index
        if not os.path.exists(path):
            continue
        text = subprocess.run(
            ["media-ctl", "-d", path, "-p"], capture_output=True, text=True
        ).stdout
        if "sc450ai" not in text:
            continue
        MEDIA = path
        for block in text.split("- entity "):
            if "stream_cif_mipi_id0" not in block:
                continue
            for line in block.splitlines():
                if "/dev/video" in line:
                    VIDEO = "/dev/" + line.split("/dev/")[-1].split()[0]
                    return
        raise RuntimeError("sc450ai graph has no capture node\n" + text)
    raise RuntimeError("sc450ai is not in a media graph")


def dphy_name():
    text = sh("media-ctl", "-d", MEDIA, "-p")
    for block in text.split("- entity "):
        first = block.splitlines()[0] if block.strip() else ""
        if "csi2-dphy" not in first:
            continue
        return first.split(":", 1)[1].rsplit("(", 1)[0].strip()
    raise RuntimeError("no csi2-dphy in the sc450ai graph")


def prepare():
    find_nodes()
    dphy = dphy_name()
    fmt = "SBGGR10_1X10/2688x1520"
    for entity, pad in (
        ("sc450ai 3-0030", "0"),
        (dphy, "0"),
        ("rockchip-mipi-csi2", "0"),
    ):
        sh("media-ctl", "-d", MEDIA, "-V", "'%s':%s [fmt:%s]" % (entity, pad, fmt))
    sh("media-ctl", "-d", MEDIA, "-l", "'sc450ai 3-0030':0 -> '%s':0 [1]" % dphy)
    sh(
        "v4l2-ctl",
        "-d",
        VIDEO,
        "--set-fmt-video=width=%d,height=%d,pixelformat=BG10" % (WIDTH, HEIGHT),
    )
    info = sh("v4l2-ctl", "-d", VIDEO, "--get-fmt-video")
    print(info, flush=True)
    stride = WIDTH * 5 // 4
    size = stride * HEIGHT
    for line in info.splitlines():
        if "Bytes per Line" in line:
            stride = int(line.split(":")[1])
        if "Size Image" in line:
            size = int(line.split(":")[1])
    return stride, size


def to_jpeg(rgb):
    blob = ctypes.c_void_p()
    length = ctypes.c_ulong()
    err = _tj.tjCompress2(
        _JPEG,
        ctypes.cast(rgb, ctypes.c_void_p),
        OUT_W,
        0,
        OUT_H,
        TJPF_RGB,
        ctypes.byref(blob),
        ctypes.byref(length),
        TJSAMP_420,
        80,
        0,
    )
    if err != 0 or not blob.value:
        raise RuntimeError("jpeg encode failed")
    try:
        return ctypes.string_at(blob.value, length.value)
    finally:
        _tj.tjFree(blob)


def capture(stride, size):
    global latest, fps
    proc = subprocess.Popen(
        [os.path.join(HERE, "livecap"), VIDEO],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    frames = 0
    mark = time.monotonic()
    while True:
        buf = bytearray()
        while len(buf) < OUT_BYTES:
            chunk = proc.stdout.read(OUT_BYTES - len(buf))
            if not chunk:
                err = proc.stderr.read().decode(errors="replace") if proc.stderr else ""
                raise SystemExit("livecap ended\n" + err)
            buf.extend(chunk)
        rgb = (ctypes.c_char * OUT_BYTES).from_buffer(buf)
        jpeg = to_jpeg(rgb)
        with lock:
            latest = jpeg
        frames += 1
        now = time.monotonic()
        if now - mark >= 2.0:
            fps = frames / (now - mark)
            frames = 0
            mark = now
            print("%.1f fps" % fps, flush=True)


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/stream"):
            self.send_response(200)
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
            self.end_headers()
            while True:
                with lock:
                    jpg = latest
                if not jpg:
                    time.sleep(0.02)
                    continue
                try:
                    self.wfile.write(b"--frame\r\nContent-Type: image/jpeg\r\n\r\n")
                    self.wfile.write(jpg)
                    self.wfile.write(b"\r\n")
                    self.wfile.flush()
                except Exception:
                    return
                time.sleep(0.03)
            return
        page = (
            "<html><body style='margin:0;background:#111'>"
            "<img src='/stream' style='width:100%'>"
            "</body></html>"
        ).encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(page)))
        self.end_headers()
        self.wfile.write(page)

    def log_message(self, fmt, *args):
        return


def main():
    stride, size = prepare()
    print("stride", stride, "size", size, "video", VIDEO, "port", PORT, flush=True)
    threading.Thread(target=capture, args=(stride, size), daemon=True).start()
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()


if __name__ == "__main__":
    main()
