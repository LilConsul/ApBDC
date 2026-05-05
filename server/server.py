#!/usr/bin/env python3
"""
server.py - HTTP server that receives compass heading from STM32 and serves a live HTML page.

Endpoints:
  POST /data   - receives "degree=<value>" from the STM32 HTTP client
  GET  /       - serves an HTML page that auto-refreshes and displays the latest heading
"""

from flask import Flask, request, jsonify, render_template_string
import threading
import datetime

app = Flask(__name__)

# Shared state protected by a lock
_lock = threading.Lock()
_state = {
    "degree": None,
    "updated_at": None,
}

# ---------------------------------------------------------------------------
# HTML page template (served inline to keep the project self-contained)
# ---------------------------------------------------------------------------
HTML_PAGE = """<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta http-equiv="refresh" content="2" />
  <title>STM32 Compass</title>
  <style>
    body  { font: 20px Arial; margin: 40px; background: #222; color: #0f0;
            text-align: center; }
    h1    { color: #0f0; margin-bottom: 10px; }
    .heading { font-size: 64px; margin: 20px; color: #0ff; font-weight: bold; }
    .info    { font-size: 16px; color: #888; margin: 8px; }
    .compass { width: 300px; height: 300px; margin: 20px auto; }
    .no-data { color: #f80; }
  </style>
</head>
<body>
  <h1>STM32 Magnetometer Compass</h1>

  {% if degree is none %}
    <p class="no-data">Waiting for data from STM32&hellip;</p>
  {% else %}
    <div class="heading">{{ degree }}&deg;</div>

    <svg class="compass" viewBox="0 0 300 300">
      <circle cx="150" cy="150" r="140" fill="#111" stroke="#0f0" stroke-width="3"/>
      <text x="150" y="30"  text-anchor="middle" fill="#f00" font-size="24" font-weight="bold">N</text>
      <text x="150" y="280" text-anchor="middle" fill="#0f0" font-size="20">S</text>
      <text x="270" y="155" text-anchor="middle" fill="#0f0" font-size="20">E</text>
      <text x="30"  y="155" text-anchor="middle" fill="#0f0" font-size="20">W</text>
      <g transform="rotate({{ degree }} 150 150)">
        <polygon points="150,40 145,150 150,160 155,150" fill="#f00"/>
        <polygon points="150,160 145,150 150,260 155,150" fill="#fff"/>
        <circle cx="150" cy="150" r="8" fill="#ff0"/>
      </g>
    </svg>

    <div class="info">Last update: {{ updated_at }}</div>
  {% endif %}

  <div class="info">Page auto-refreshes every 2 seconds</div>
</body>
</html>"""


# ---------------------------------------------------------------------------
# Routes
# ---------------------------------------------------------------------------

@app.route("/", methods=["GET"])
def index():
    """Serve the live compass HTML page."""
    with _lock:
        degree = _state["degree"]
        updated_at = _state["updated_at"]
    return render_template_string(HTML_PAGE, degree=degree, updated_at=updated_at)


@app.route("/data", methods=["POST"])
def receive_data():
    """
    Receive heading data from STM32.
    Expects form-encoded body: degree=<float>
    """
    degree_raw = request.form.get("degree")
    if degree_raw is None:
        return jsonify({"error": "missing 'degree' field"}), 400

    try:
        degree_value = float(degree_raw)
    except ValueError:
        return jsonify({"error": f"invalid degree value: {degree_raw!r}"}), 400

    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    with _lock:
        _state["degree"] = f"{degree_value:.1f}"
        _state["updated_at"] = now

    print(f"[{now}] Received heading: {degree_value:.1f}°")
    return jsonify({"status": "ok", "degree": degree_value}), 200


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    print("STM32 Compass Server starting on http://0.0.0.0:5000")
    print("  POST /data  — receive heading from STM32")
    print("  GET  /      — view live compass page")
    app.run(host="0.0.0.0", port=5000, debug=False)
