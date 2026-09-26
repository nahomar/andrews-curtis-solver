"""Scoring server for acs --guide: unix socket; request = uint32 n + n*66 token bytes; reply = n float32 (moves to go).
One lock around the model (MPS is not thread-safe); each solver thread keeps its own connection."""
import socket, struct, sys, threading, os, numpy as np, torch
sys.path.insert(0, '/Users/nahom/acc/vbias')
from model import load
path, sock_path = sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else '/tmp/acs_guide.sock'
DEV = 'mps' if torch.backends.mps.is_available() else 'cpu'
model = load(path, DEV); lock = threading.Lock()
def recvall(c, n):
    b = bytearray()
    while len(b) < n:
        ch = c.recv(n - len(b))
        if not ch: raise ConnectionError
        b += ch
    return bytes(b)
def serve(c):
    try:
        while True:
            n = struct.unpack('<I', recvall(c, 4))[0]
            X = np.frombuffer(recvall(c, n * 66), np.uint8).reshape(n, 66).astype(np.int64)
            with lock, torch.no_grad():
                out = []
                for i in range(0, n, 4096):
                    out.append(model(torch.from_numpy(X[i:i + 4096]).to(DEV)).float().cpu().numpy())
            c.sendall(np.concatenate(out).astype('<f4').tobytes() if out else b'')
    except (ConnectionError, OSError): pass
    finally: c.close()
if os.path.exists(sock_path): os.unlink(sock_path)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); s.bind(sock_path); s.listen(64)
print('guide server ready on', sock_path, 'device', DEV, flush=True)
while True:
    c, _ = s.accept(); threading.Thread(target=serve, args=(c,), daemon=True).start()
