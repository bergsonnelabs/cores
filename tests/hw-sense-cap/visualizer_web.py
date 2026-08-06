#!/usr/bin/env python3
"""Web-based live touch visualizer for the Sense.CAP 2x3 surface.

Reads the hw-sense-cap firmware serial stream and serves a live view at
http://localhost:8765 — surface heatmap, finger + trail, delta bars with
threshold, event ticker. Stdlib + pyserial only; the page is a single
HTML5 canvas fed by Server-Sent Events.

Usage:  python3 visualizer_web.py [serial-port]
"""

import glob
import json
import sys
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import serial

PORT = 8765

state_lock = threading.Lock()
ser_handle = None
state = {
    "t": 0, "nf": 0, "x": 0, "y": 0, "strength": 0,
    "zone": -1, "touchbits": 0, "deltas": [0] * 6, "alive": False,
}
events = deque(maxlen=200)
event_seq = 0


def serial_port():
    if len(sys.argv) > 1:
        return sys.argv[1]
    ports = glob.glob("/dev/tty.usbmodem*")
    return ports[0] if ports else None


def reader():
    global event_seq
    while True:
        port = serial_port()
        if not port:
            time.sleep(1)
            continue
        try:
            s = serial.Serial(port, 115200, timeout=1)
            global ser_handle
            ser_handle = s
            with state_lock:
                state["alive"] = True
            while True:
                line = s.readline().decode(errors="replace").strip()
                if not line:
                    continue
                parts = line.split(",")
                if parts[0] == "D" and len(parts) >= 14:
                    try:
                        with state_lock:
                            state.update(
                                t=int(parts[1]), nf=int(parts[2]),
                                x=int(parts[3]), y=int(parts[4]),
                                strength=int(parts[5]), zone=int(parts[6]),
                                touchbits=int(parts[7], 16),
                                deltas=[int(v) for v in parts[8:14]])
                    except ValueError:
                        pass
                elif parts[0] == "E" and len(parts) >= 2:
                    with state_lock:
                        event_seq += 1
                        events.append({"i": event_seq,
                                       "t": time.strftime("%H:%M:%S"),
                                       "msg": " ".join(parts[1:])})
                elif "recipe" in line:
                    with state_lock:
                        event_seq += 1
                        events.append({"i": event_seq,
                                       "t": time.strftime("%H:%M:%S"),
                                       "msg": line})
        except (serial.SerialException, OSError):
            with state_lock:
                state["alive"] = False
            time.sleep(1)


PAGE = """<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Sense.CAP touch</title>
<style>
  body { background:#101418; color:#c0cad4; font-family:Menlo,monospace;
         display:flex; gap:16px; padding:16px; }
  #wrap { flex:0 0 auto; }
  canvas { background:#0a0d10; border-radius:8px; }
  #ticker { flex:1; background:#0a0d10; border-radius:8px; padding:10px;
            height:520px; overflow-y:auto; font-size:12px; color:#9fd8a0; }
  #ticker div { padding:1px 0; }
  h3 { margin:4px 0 10px; color:#8090a0; font-size:13px; }
  button { background:#1d242c; color:#c0cad4; border:1px solid #39424d;
           border-radius:5px; font-family:Menlo,monospace; font-size:12px;
           padding:4px 10px; margin-right:4px; cursor:pointer; }
  button:hover { background:#2a333d; }
</style></head>
<body>
<div id="wrap">
  <h3 id="hdr">Sense.CAP 2&times;3 &mdash; connecting&hellip;</h3>
  <div style="margin-bottom:8px">
    <button onclick="cmd('r')">Re-ATI (re-baseline)</button>
    sensitivity:
    <button onclick="cmd('s1')">1</button><button onclick="cmd('s2')">2</button><button onclick="cmd('s3')">3</button><button onclick="cmd('s4')">4</button><button onclick="cmd('s5')">5</button>
  </div>
  <canvas id="cv" width="820" height="560"></canvas>
</div>
<div>
  <h3>events</h3>
  <div id="ticker"></div>
</div>
<script>
const XR=512, YR=256, COLS=3, ROWS=2, SC=1.6;
const W=XR*SC, H=YR*SC, BAR=120, PAD=10;
const DELTA_FULL=200, THRESH=Math.floor(900*8/128);
const cv=document.getElementById('cv'), ctx=cv.getContext('2d');
const ticker=document.getElementById('ticker'), hdr=document.getElementById('hdr');
let trail=[], seen=0;

function heat(d){
  const f=Math.max(0,Math.min(1,d/DELTA_FULL));
  const r=0x20+f*(0xff-0x20), g=0x2a+f*(0x8a-0x2a), b=0x33+f*(0x1a-0x33);
  return `rgb(${r|0},${g|0},${b|0})`;
}

function draw(s){
  ctx.clearRect(0,0,cv.width,cv.height);
  const cw=W/COLS, ch=H/ROWS;
  for(let c=0;c<COLS;c++) for(let r=0;r<ROWS;r++){
    const z=c*ROWS+r, x0=PAD+c*cw, y0=PAD+r*ch;
    const touched=(s.touchbits>>z)&1;
    ctx.fillStyle=heat(s.deltas[z]);
    ctx.fillRect(x0+3,y0+3,cw-6,ch-6);
    ctx.strokeStyle=touched?'#eeeeee':'#39424d';
    ctx.lineWidth=touched?3:1;
    ctx.strokeRect(x0+3,y0+3,cw-6,ch-6);
    ctx.fillStyle='#8090a0'; ctx.font='bold 13px Menlo';
    ctx.fillText(z, x0+10, y0+20);
    ctx.fillStyle='#c0cad4'; ctx.font='11px Menlo';
    ctx.fillText((s.deltas[z]>=0?'+':'')+s.deltas[z], x0+cw-52, y0+ch-12);
  }
  // trail + finger
  trail.forEach((p,i)=>{
    const f=i/Math.max(1,trail.length), r=2+4*f;
    ctx.fillStyle=`rgba(${64+f*143|0},220,60,${0.15+0.35*f})`;
    ctx.beginPath();
    ctx.arc(PAD+p[0]*SC,PAD+p[1]*SC,r,0,7); ctx.fill();
  });
  if(s.nf>0 && s.x<65535){
    const fx=PAD+s.x*SC, fy=PAD+s.y*SC, r=10+Math.min(20,s.strength/40);
    ctx.strokeStyle='#ffe066'; ctx.lineWidth=3;
    ctx.beginPath(); ctx.arc(fx,fy,r,0,7); ctx.stroke();
    ctx.fillStyle='#ffe066'; ctx.font='12px Menlo';
    ctx.fillText(`(${s.x},${s.y}) z${s.zone} s${s.strength}`,
                 Math.min(fx,W-140), Math.max(16,fy-18));
  }
  // delta bars
  const top=PAD+H+12, bw=W/6,
        span=Math.max(DELTA_FULL,...s.deltas.map(Math.abs)),
        mid=top+(BAR-24)/2;
  for(let z=0;z<6;z++){
    const x0=PAD+z*bw+10, h=(s.deltas[z]/span)*(BAR-28)/2;
    ctx.fillStyle=heat(Math.abs(s.deltas[z]));
    ctx.fillRect(x0,Math.min(mid,mid-h),bw-20,Math.abs(h));
    ctx.fillStyle='#8090a0'; ctx.font='11px Menlo';
    ctx.fillText('ch'+z, x0+(bw-20)/2-12, top+BAR-8);
  }
  const ty=mid-(THRESH/span)*(BAR-28)/2;
  ctx.strokeStyle='#ff6666'; ctx.setLineDash([4,3]); ctx.lineWidth=1;
  ctx.beginPath(); ctx.moveTo(PAD,ty); ctx.lineTo(PAD+W,ty); ctx.stroke();
  ctx.setLineDash([]);
  ctx.strokeStyle='#39424d';
  ctx.beginPath(); ctx.moveTo(PAD,mid); ctx.lineTo(PAD+W,mid); ctx.stroke();
  ctx.fillStyle='#ff6666'; ctx.font='10px Menlo';
  ctx.fillText('thr '+THRESH, PAD+W-70, ty-5);
}

function cmd(c){ fetch('/cmd?c='+c); }
const es=new EventSource('/events');
es.onmessage=(m)=>{
  const d=JSON.parse(m.data);
  hdr.innerHTML='Sense.CAP 2&times;3 &mdash; '+(d.alive?'live':'no serial');
  if(d.nf>0 && d.x<65535){ trail.push([d.x,d.y]); if(trail.length>40) trail.shift(); }
  else if(trail.length) trail.shift();
  draw(d);
  for(const e of d.events){
    if(e.i>seen){
      seen=e.i;
      const div=document.createElement('div');
      div.textContent=e.t+' '+e.msg;
      ticker.prepend(div);
      while(ticker.childNodes.length>120) ticker.removeChild(ticker.lastChild);
    }
  }
};
</script></body></html>
"""


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_GET(self):
        if self.path == "/":
            body = PAGE.encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path == "/events":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            last_seq = 0
            try:
                while True:
                    with state_lock:
                        fresh = [e for e in events if e["i"] > last_seq]
                        if fresh:
                            last_seq = fresh[-1]["i"]
                        payload = dict(state, events=fresh)
                    self.wfile.write(
                        f"data: {json.dumps(payload)}\n\n".encode())
                    self.wfile.flush()
                    time.sleep(0.04)
            except (BrokenPipeError, ConnectionResetError):
                return
        elif self.path.startswith("/cmd?c="):
            cmd = self.path.split("=", 1)[1][:4]
            ok = False
            if ser_handle is not None:
                try:
                    ser_handle.write(cmd.encode())
                    ok = True
                except (serial.SerialException, OSError):
                    pass
            body = b"ok" if ok else b"no-serial"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        else:
            self.send_response(404)
            self.end_headers()


def main():
    threading.Thread(target=reader, daemon=True).start()
    srv = ThreadingHTTPServer(("127.0.0.1", PORT), Handler)
    print(f"serving http://localhost:{PORT}")
    srv.serve_forever()


if __name__ == "__main__":
    main()
