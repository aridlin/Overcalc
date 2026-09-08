"""Verify exported output matches normal CLI output after the TUI restores the terminal."""
import os, pty, fcntl, termios, struct, subprocess, select, time, sys
exe=sys.argv[1]
for mode in ('keyboard','mouse','derivative'):
    master,slave=pty.openpty()
    fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',24,80,0,0))
    old=termios.tcgetattr(slave)
    proc=subprocess.Popen([exe,'--tui','--no-color'],stdin=slave,stdout=slave,stderr=slave)
    output=bytearray()
    def drain():
        end=time.monotonic()+.3
        while time.monotonic()<end:
            if select.select([master],[],[],.02)[0]:output.extend(os.read(master,65536))
    def send(keys):os.write(master,keys);drain()
    try:
        drain();send(b'\x0f')
        assert proc.poll() is None,'empty editor unexpectedly exited'
        assert b'Fill the empty fields' in output
        if mode=='derivative':
            send(b'x^2\x04');expr='(x)^{2}';extra=['--derive','x']
        else:
            send(b'\x061');send(b'\x0f')
            assert proc.poll() is None,'incomplete fraction unexpectedly exited'
            send(b'\t4');expr=r'\frac{1}{4}';extra=[]
        if mode=='mouse':send(b'\x1b[<0;66;2M')
        else:send(b'\x0f')
        proc.wait(timeout=3);drain()
        assert proc.returncode==0
        assert termios.tcgetattr(slave)==old,'terminal flags not restored'
        assert b'\x1b[?1049l' in output
        exported=bytes(output).split(b'\x1b[?1049l',1)[1].replace(b'\r\n',b'\n')
        expected=subprocess.check_output([exe,'--no-color',*extra,expr])
        assert exported==expected,(mode,exported,expected)
    finally:
        if proc.poll() is None:proc.kill();proc.wait()
        os.close(master);os.close(slave)
print('Print-and-exit keyboard, mouse, incomplete fields and derivative match normal CLI output')
