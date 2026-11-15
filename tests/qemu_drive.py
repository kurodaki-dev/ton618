#!/usr/bin/env python3
"""Drives QEMU through its monitor socket: boots an ISO, moves the mouse, clicks, types, takes screenshots.
usage: qemu_drive.py image.iso out_dir script   where script is steps separated by ';':
  wait:SECONDS  move:X,Y (absolute, from the top-left corner)  click  press  release  key:NAME  type:TEXT  shot:NAME
"""
import os, socket, subprocess, sys, time

def main():
    iso, out, script = sys.argv[1], sys.argv[2], sys.argv[3]
    disk = sys.argv[4] if len(sys.argv) > 4 else None
    os.makedirs(out, exist_ok=True)
    sock = os.path.join(out, "mon.sock")
    if os.path.exists(sock): os.remove(sock)
    cmd = ["qemu-system-i386", "-cdrom", iso, "-display", "none", "-serial", "file:" + os.path.join(out, "serial.txt"),
           "-monitor", f"unix:{sock},server,nowait", "-no-reboot"]
    if disk: cmd += ["-hda", disk]
    q = subprocess.Popen(cmd)
    for _ in range(50):
        if os.path.exists(sock): break
        time.sleep(0.1)
    s = socket.socket(socket.AF_UNIX); s.connect(sock); s.settimeout(2)
    def mon(line):
        s.sendall((line + "\n").encode())
        time.sleep(0.05)
        try: s.recv(65536)
        except Exception: pass
    pos = [512, 384]
    try:
        for step in script.split(";"):
            step = step.strip()
            if not step: continue
            name, _, arg = step.partition(":")
            if name == "wait": time.sleep(float(arg))
            elif name == "move":
                x, y = (int(v) for v in arg.split(","))
                # a relative mouse: push it to the corner first, then to the target, in small steps
                if pos is None or arg == "0,0":
                    for _ in range(12): mon("mouse_move -200 -200")
                    pos = [0, 0]
                dx, dy = x - pos[0], y - pos[1]
                while dx or dy:
                    sx = max(-120, min(120, dx)); sy = max(-120, min(120, dy))
                    mon(f"mouse_move {sx} {sy}"); dx -= sx; dy -= sy
                pos = [x, y]
            elif name == "press": mon("mouse_button 1")
            elif name == "release": mon("mouse_button 0")
            elif name == "click": mon("mouse_button 1"); time.sleep(0.15); mon("mouse_button 0"); time.sleep(0.4)
            elif name == "key": mon("sendkey " + arg); time.sleep(0.3)
            elif name == "type":
                for ch in arg:
                    mon("sendkey " + {" ": "spc", ".": "dot", "+": "shift-equal", "-": "minus", "*": "shift-8", "/": "slash", "=": "equal", ":": "shift-semicolon"}.get(ch, ch)); time.sleep(0.12)
            elif name == "shot":
                ppm = os.path.join(out, arg + ".ppm")
                mon("screendump " + ppm); time.sleep(0.5)
                try:
                    from PIL import Image
                    Image.open(ppm).save(os.path.join(out, arg + ".png"))
                except Exception: pass
            else: sys.exit("unknown step " + step)
    finally:
        q.terminate()
        try: q.wait(5)
        except Exception: q.kill()

main()
