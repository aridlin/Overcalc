"""Linux PTY integration checks; requires Python pyte. Pass the binary path."""
import atexit, codecs, os, pty, fcntl, termios, struct, subprocess, select, time, sys
import pyte
exe=sys.argv[1]
master,slave=pty.openpty()
fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',24,80,0,0))
old=termios.tcgetattr(slave)
p=subprocess.Popen([exe,'--tui'],stdin=slave,stdout=slave,stderr=slave)
def cleanup():
    if p.poll() is None:
        p.terminate()
        p.wait(timeout=3)
atexit.register(cleanup)
screen=pyte.Screen(80,24); stream=pyte.Stream(screen)
decoder=codecs.getincrementaldecoder('utf8')()
def drain():
    end=time.monotonic()+.4
    while time.monotonic()<end:
        if select.select([master],[],[],.02)[0]:
            stream.feed(decoder.decode(os.read(master,65536)))
def send(b):
    os.write(master,b);drain()
def text(): return '\n'.join(screen.display)
def expect(s):
    assert s in text(),s+'\n'+text()
def click(x,y):send(f'\x1b[<0;{x+1};{y+1}M\x1b[<0;{x+1};{y+1}m'.encode())
def click_label(label):
    for y,line in enumerate(screen.display):
        if label in line:
            click(line.index(label),y);return
    raise AssertionError('Missing '+label+'\n'+text())
drain();expect('OverCalc');expect('Fraction');expect('Superscript');expect('───')
send(b'\x06' + b'1\t2');expect('=  1/2')
# Unicode must occupy terminal cells, not UTF-8 byte positions.
assert '\ufffd' not in text()
assert screen.buffer[screen.cursor.y][screen.cursor.x-1].data=='2'
den_x,den_y=screen.cursor.x-1,screen.cursor.y
send(b'\x1b[A\x1b[F2');expect('=  6')
click(den_x,den_y);send(b'\x1b[F4');expect('=  1/2')
send(b'\x1a');expect('=  6')
send(b'\x19');expect('=  1/2')
send(b'\x15' + b'2^3\t+1');expect('=  9')
send(b'\x15\x10\x1b[C\x1b[C\r9');expect('=  3');expect('√')
send(b'\x15');click_label('Fraction');send(b'1\t4');expect('=  1/4')
# Cards are clickable over the actual miniature expression, not just the caption.
send(b'\x15')
for y,line in enumerate(screen.display):
    if 'Fraction' in line:
        click(line.index('Fraction')+2,y-2);break
send(b'2\t4');expect('=  1/2')
# Page controls reveal later symbols; UTF-8 constants remain editable and evaluable.
send(b'\x15\x10'+b'\x1b[C'*12+b'\r');expect('π')
send(b'/2');expect('≈')
# Resize should redraw all borders and keep caret, equation, and palette in bounds.
fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',18,50,0,0)); screen.resize(18,50);drain();expect('OverCalc');expect('RESULT');expect('INSERT')
assert 0<=screen.cursor.x<50 and 0<=screen.cursor.y<18
send(b'\x15\x06' + b'1\t4');expect('=  1/4')
# Check opaque frame background and intact Unicode after edits and resizing.
assert screen.buffer[0][0].bg=='111722'
assert '\ufffd' not in text()
send(b'\x03');p.wait(timeout=3)
assert p.returncode==0
assert termios.tcgetattr(slave)==old,'terminal mode not restored'
os.close(master);os.close(slave)
print('PTY Unicode, keyboard, mouse, formula cards, paging, undo/redo, resize and terminal restoration passed')
