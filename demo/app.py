#!/usr/bin/env python3
"""
国标麻将算番 Web 服务
零依赖: 仅使用 Python 标准库 (http.server)
后端通过 subprocess 调用编译好的 demo 二进制 (--json 模式)
"""
import json
import os
import subprocess
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# 路径
HERE = os.path.dirname(os.path.abspath(__file__))
DEMO_BIN = os.path.join(HERE, "demo")
INDEX_HTML = os.path.join(HERE, "index.html")
ASSETS_DIR = os.path.join(HERE, "assets")
PORT = 8000


def run_calculate(hand: str, win: str, prevalent: str, seat: str, flower: int) -> dict:
    """调用 demo 二进制, 返回解析后的 dict"""
    try:
        result = subprocess.run(
            [DEMO_BIN, "--json", hand,
             f"win={win}", f"prevalent={prevalent}",
             f"seat={seat}", f"flower={flower}"],
            capture_output=True, text=True, timeout=5,
        )
        out = result.stdout.strip()
        if not out:
            return {"ok": False, "error": f"后端无输出 (stderr={result.stderr.strip()})",
                    "points": 0, "base_score": 0, "fans": [], "normalized": ""}
        return json.loads(out)
    except json.JSONDecodeError as e:
        return {"ok": False, "error": f"解析失败: {e}", "points": 0,
                "base_score": 0, "fans": [], "normalized": ""}
    except Exception as e:
        return {"ok": False, "error": f"内部错误: {e}", "points": 0,
                "base_score": 0, "fans": [], "normalized": ""}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass  # 静默日志

    def _send_json(self, obj: dict, code: int = 200):
        data = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(data)

    def _send_file(self, path: str, content_type: str):
        try:
            with open(path, "rb") as f:
                data = f.read()
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "public, max-age=86400")
            self.end_headers()
            self.wfile.write(data)
        except FileNotFoundError:
            self.send_error(404, "Not Found")

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path == "/" or path == "/index.html":
            self._send_file(INDEX_HTML, "text/html; charset=utf-8")
            return
        # 静态资源: /assets/...
        if path.startswith("/assets/"):
            # 防路径穿越
            rel = os.path.normpath(path.lstrip("/"))
            full = os.path.join(HERE, rel)
            if not full.startswith(ASSETS_DIR):
                self.send_error(403, "Forbidden")
                return
            ext = os.path.splitext(full)[1].lower()
            ct = {".svg": "image/svg+xml", ".png": "image/png",
                  ".jpg": "image/jpeg", ".gif": "image/gif"}.get(ext, "application/octet-stream")
            self._send_file(full, ct)
            return
        if path == "/api/calculate":
            qs = urllib.parse.parse_qs(parsed.query)
            try:
                res = run_calculate(
                    hand=qs.get("hand", [""])[0],
                    win=qs.get("win", ["discard"])[0],
                    prevalent=qs.get("prevalent", ["east"])[0],
                    seat=qs.get("seat", ["east"])[0],
                    flower=int(qs.get("flower", ["0"])[0]),
                )
                self._send_json(res)
            except Exception as e:
                self._send_json({"ok": False, "error": str(e)}, 500)
            return
        self.send_error(404, "Not Found")

    def do_POST(self):
        if self.path == "/api/calculate":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length).decode("utf-8") if length else ""
            try:
                payload = json.loads(body) if body else {}
                res = run_calculate(
                    hand=payload.get("hand", ""),
                    win=payload.get("win", "discard"),
                    prevalent=payload.get("prevalent", "east"),
                    seat=payload.get("seat", "east"),
                    flower=int(payload.get("flower", 0)),
                )
                self._send_json(res)
            except Exception as e:
                self._send_json({"ok": False, "error": str(e)}, 500)
            return
        self.send_error(404, "Not Found")


def main():
    if not os.path.exists(DEMO_BIN):
        print(f"❌ 找不到 demo 二进制: {DEMO_BIN}")
        print("   请先在 demo/ 目录运行: make")
        return
    server = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    print(f"🀄 国标麻将算番 Web 服务已启动")
    print(f"   本地访问: http://localhost:{PORT}")
    print(f"   按 Ctrl+C 停止")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n已停止")
        server.shutdown()


if __name__ == "__main__":
    main()
