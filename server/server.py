#!/usr/bin/env python3
"""
server.py - HTTP server that receives compass heading from STM32 and serves a live HTML page.

Endpoints:
  POST /data   - receives "degree=<value>" from the STM32 HTTP client
  GET  /       - serves an HTML page that auto-refreshes and displays the latest heading

Run with:
  uv run uvicorn server:app --host 0.0.0.0 --port 5000
or:
  uv run server.py
"""

import datetime
import threading

import uvicorn
from fastapi import FastAPI, Form, HTTPException
from fastapi.responses import HTMLResponse, JSONResponse

app = FastAPI(title="STM32 Compass Server")

# Shared state protected by a lock
_lock = threading.Lock()
_state: dict = {
    "degree": None,
    "updated_at": None,
}

# ---------------------------------------------------------------------------
# HTML page template
# ---------------------------------------------------------------------------
_HTML_TEMPLATE = """\
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta http-equiv="refresh" content="2" />
  <title>STM32 Compass</title>
  <style>
    body     {{ font: 20px Arial; margin: 40px; background: #222; color: #0f0;
               text-align: center; }}
    h1       {{ color: #0f0; margin-bottom: 10px; }}
    .heading {{ font-size: 64px; margin: 20px; color: #0ff; font-weight: bold; }}
    .info    {{ font-size: 16px; color: #888; margin: 8px; }}
    .compass {{ width: 300px; height: 300px; margin: 20px auto; }}
    .no-data {{ color: #f80; }}
  </style>
</head>
<body>
  <h1>STM32 Magnetometer Compass</h1>
  {body}
  <div class="info">Page auto-refreshes every 2&nbsp;seconds</div>
</body>
</html>"""

_NO_DATA_BODY = '<p class="no-data">Waiting for data from STM32&hellip;</p>'

_DATA_BODY = """\
  <div class="heading">{degree}&deg;</div>
  <svg class="compass" viewBox="0 0 300 300">
    <circle cx="150" cy="150" r="140" fill="#111" stroke="#0f0" stroke-width="3"/>
    <text x="150" y="30"  text-anchor="middle" fill="#f00" font-size="24" font-weight="bold">N</text>
    <text x="150" y="280" text-anchor="middle" fill="#0f0" font-size="20">S</text>
    <text x="270" y="155" text-anchor="middle" fill="#0f0" font-size="20">E</text>
    <text x="30"  y="155" text-anchor="middle" fill="#0f0" font-size="20">W</text>
    <g transform="rotate({degree} 150 150)">
      <polygon points="150,40 145,150 150,160 155,150" fill="#f00"/>
      <polygon points="150,160 145,150 150,260 155,150" fill="#fff"/>
      <circle cx="150" cy="150" r="8" fill="#ff0"/>
    </g>
  </svg>
  <div class="info">Last update: {updated_at}</div>"""


def _render_page(degree: str | None, updated_at: str | None) -> str:
    if degree is None:
        body = _NO_DATA_BODY
    else:
        body = _DATA_BODY.format(degree=degree, updated_at=updated_at)
    return _HTML_TEMPLATE.format(body=body)


# ---------------------------------------------------------------------------
# Routes
# ---------------------------------------------------------------------------

@app.get("/", response_class=HTMLResponse)
async def index() -> HTMLResponse:
    """Serve the live compass HTML page."""
    with _lock:
        degree = _state["degree"]
        updated_at = _state["updated_at"]
    return HTMLResponse(_render_page(degree, updated_at))


@app.post("/data")
async def receive_data(degree: str = Form(...)) -> JSONResponse:
    """
    Receive heading data from STM32.
    Expects form-encoded body: degree=<float>
    """
    try:
        degree_value = float(degree)
    except ValueError:
        raise HTTPException(status_code=400,
                            detail=f"invalid degree value: {degree!r}")

    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    with _lock:
        _state["degree"] = f"{degree_value:.1f}"
        _state["updated_at"] = now

    print(f"[{now}] Received heading: {degree_value:.1f}\u00b0")
    return JSONResponse({"status": "ok", "degree": degree_value})


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    print("STM32 Compass Server starting on http://0.0.0.0:5000")
    print("  POST /data  \u2014 receive heading from STM32")
    print("  GET  /      \u2014 view live compass page")
    uvicorn.run("server:app", host="0.0.0.0", port=5000, reload=False)
