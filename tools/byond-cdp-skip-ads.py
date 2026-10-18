#!/usr/bin/env python3
"""Fast-forward the BYOND advertisement page through WebView2's CDP port.

This is a test-runner helper, not a BYOND or Wine patch.  It finds the real
DreamSeeker advertisement document and dispatches the same pager://ok action
used by the page's Continue link.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import socket
import struct
import sys
import time
import urllib.error
import urllib.request


class WebSocketError(RuntimeError):
    pass


def read_exact(sock: socket.socket, size: int) -> bytes:
    chunks = []
    remaining = size
    while remaining:
        chunk = sock.recv(remaining)
        if not chunk:
            raise WebSocketError("WebSocket closed while reading a frame")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def websocket_connect(url: str, timeout: float) -> socket.socket:
    if not url.startswith("ws://"):
        raise WebSocketError(f"unsupported DevTools URL: {url}")

    address = url[5:]
    host_port, path = address.split("/", 1)
    host, port_text = host_port.rsplit(":", 1)
    sock = socket.create_connection((host, int(port_text)), timeout=timeout)
    sock.settimeout(timeout)

    key = base64.b64encode(os.urandom(16)).decode("ascii")
    request = (
        f"GET /{path} HTTP/1.1\r\n"
        f"Host: {host_port}\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n"
    ).encode("ascii")
    sock.sendall(request)

    response = b""
    while b"\r\n\r\n" not in response:
        response += sock.recv(4096)
        if len(response) > 65536:
            sock.close()
            raise WebSocketError("oversized WebSocket handshake")
    if not response.startswith(b"HTTP/1.1 101"):
        sock.close()
        raise WebSocketError(response.split(b"\r\n", 1)[0].decode("latin1"))

    expected = base64.b64encode(
        hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")).digest()
    ).decode("ascii")
    headers = response.split(b"\r\n\r\n", 1)[0].decode("latin1").split("\r\n")[1:]
    values = {}
    for header in headers:
        if ":" in header:
            name, value = header.split(":", 1)
            values[name.lower()] = value.strip()
    if values.get("sec-websocket-accept") != expected:
        sock.close()
        raise WebSocketError("invalid WebSocket handshake response")
    return sock


def websocket_send_text(sock: socket.socket, value: str) -> None:
    payload = value.encode("utf-8")
    websocket_send_frame(sock, 0x1, payload)


def websocket_send_frame(sock: socket.socket, opcode: int, payload: bytes) -> None:
    mask = os.urandom(4)
    masked = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
    length = len(masked)
    if length < 126:
        header = bytes((0x80 | opcode, 0x80 | length))
    elif length <= 0xFFFF:
        header = bytes((0x80 | opcode, 0x80 | 126)) + struct.pack("!H", length)
    else:
        header = bytes((0x80 | opcode, 0x80 | 127)) + struct.pack("!Q", length)
    sock.sendall(header + mask + masked)


def websocket_receive(sock: socket.socket) -> tuple[int, bytes]:
    first, second = read_exact(sock, 2)
    fin = first & 0x80
    opcode = first & 0x0F
    masked = second & 0x80
    length = second & 0x7F
    if length == 126:
        length = struct.unpack("!H", read_exact(sock, 2))[0]
    elif length == 127:
        length = struct.unpack("!Q", read_exact(sock, 8))[0]
    mask = read_exact(sock, 4) if masked else b""
    payload = read_exact(sock, length)
    if masked:
        payload = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
    if not fin:
        raise WebSocketError("fragmented DevTools response is unsupported")
    return opcode, payload


def websocket_command(sock: socket.socket, command_id: int, method: str, params: dict) -> dict:
    websocket_send_text(sock, json.dumps({"id": command_id, "method": method, "params": params}))
    while True:
        opcode, payload = websocket_receive(sock)
        if opcode == 0x9:  # ping
            websocket_send_frame(sock, 0xA, payload)
            continue
        if opcode == 0x8:
            raise WebSocketError("DevTools target closed")
        if opcode != 0x1:
            continue
        message = json.loads(payload.decode("utf-8"))
        if message.get("id") == command_id:
            return message


def targets(port: int, timeout: float) -> list[dict]:
    for endpoint in (f"http://127.0.0.1:{port}/json/list", f"http://127.0.0.1:{port}/json"):
        try:
            with urllib.request.urlopen(endpoint, timeout=timeout) as response:
                value = json.load(response)
            return value if isinstance(value, list) else []
        except (OSError, ValueError, urllib.error.URLError):
            continue
    return []


def is_ad_target(target: dict) -> bool:
    url = str(target.get("url", "")).lower()
    title = str(target.get("title", "")).lower()
    return (
        target.get("type") == "page"
        and ("ad.html" in url or "your game is starting" in title)
    )


SKIP_SCRIPT = r"""(() => {
  const link = [...document.querySelectorAll('a')]
    .find(a => /^pager:\/\/ok(?:\?|$)/i.test(a.href));
  if (link) {
    const href = link.href;
    link.click();
    return { action: 'click', href };
  }
  window.location.href = 'pager://ok?type=static';
  return { action: 'navigate', href: 'pager://ok?type=static' };
})()"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--timeout", type=float, default=70.0)
    parser.add_argument("--interval", type=float, default=0.25)
    args = parser.parse_args()

    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline:
        for target in targets(args.port, min(1.0, args.interval * 2)):
            if not is_ad_target(target):
                continue
            target_url = target.get("url", "")
            print(f"skip_ads_target_found url={target_url!r}", flush=True)
            try:
                sock = websocket_connect(target["webSocketDebuggerUrl"], 5.0)
                try:
                    print("skip_ads_dispatch=pager://ok?type=static", flush=True)
                    result = websocket_command(
                        sock,
                        1,
                        "Runtime.evaluate",
                        {"expression": SKIP_SCRIPT, "returnByValue": True},
                    )
                    print(f"skip_ads_result={json.dumps(result, sort_keys=True)}", flush=True)
                finally:
                    sock.close()
            except (KeyError, OSError, WebSocketError, ValueError) as error:
                # A custom-scheme navigation may close the target before CDP
                # can return the Runtime.evaluate response. The dispatch line
                # above is still the useful evidence in that case.
                print(f"skip_ads_cdp_error={error}", flush=True)
            return 0
        time.sleep(args.interval)

    print(f"skip_ads_timeout port={args.port}", file=sys.stderr, flush=True)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
