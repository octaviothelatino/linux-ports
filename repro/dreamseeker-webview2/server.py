#!/usr/bin/env python3
"""Tiny local HTTP server matching BYOND's WebView2 document URLs."""

import argparse
import http.server
import pathlib
import socketserver


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=pathlib.Path)
    parser.add_argument("--port-file", required=True, type=pathlib.Path)
    args = parser.parse_args()

    handler = lambda *handler_args: QuietHandler(
        *handler_args, directory=str(args.root)
    )
    with socketserver.ThreadingTCPServer(("127.0.0.1", 0), handler) as server:
        server.daemon_threads = True
        args.port_file.write_text(str(server.server_address[1]), encoding="ascii")
        server.serve_forever()


if __name__ == "__main__":
    main()
