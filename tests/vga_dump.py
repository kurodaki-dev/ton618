# Reads the 80x25 text screen of a QEMU virtual machine through its monitor socket (used by tests/atome-cli.sh).
import socket, time, re, sys
def screen(sock):
    sock.send(b"xp /2000hx 0xb8000\n"); time.sleep(0.8)
    data = b""
    sock.settimeout(1.0)
    try:
        while True:
            chunk = sock.recv(65536)
            if not chunk: break
            data += chunk
    except Exception: pass
    cells=[]
    for line in data.decode(errors="ignore").splitlines():
        m=re.match(r'^[0-9a-f]+:\s+((?:0x[0-9a-f]+\s*)+)$', line.strip())
        if m: cells += [int(x,16) for x in m.group(1).split()]
    rows=[]
    for r in range(25):
        row=cells[r*80:(r+1)*80]
        rows.append(''.join(chr(c & 0xff) if 32 <= (c & 0xff) < 127 else ('#' if (c & 0xff) else ' ') for c in row).rstrip())
    return rows
