#!/usr/bin/env python3
"""
Unpack the transport form into the current directory.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile

if os.path.exists('/usr/bin/pigz'):
    GZIP = 'pigz'
else:
    GZIP = 'gzip'


def read_meta_data(size, f):
    with open('desc.xml', 'wb') as f2:
        f2.write(f.read(size))
        print("wrote desc.xml.")

def read_index(size, f):
    types = {
        0: 'regular',
        1: 'directory',
        2: 'link',
        3: 'block',
        4: 'char',
        5: 'socket',
        6: 'pipe',
    }

    with open('index', 'w', encoding='utf8') as f2:
        while size > 0:
            entry_size = 0

            type_ = struct.unpack('<B', f.read(1))[0]
            uid = struct.unpack('<I', f.read(4))[0]
            gid = struct.unpack('<I', f.read(4))[0]
            mode = struct.unpack('<H', f.read(2))[0]
            size_ = struct.unpack('<I', f.read(4))[0]
            sha1 = ''.join('%02x' % b for b in f.read(20))
            entry_size += 35

            path = b''
            while True:
                c = f.read(1)
                entry_size += 1

                if c == b'\0':
                    break
                path += c

            path = path.decode('utf8')

            f2.write("%-10s:%05d:%05d:%04o:%010d:%s:%s\n" % (
                types.get(type_, '?'), uid, gid, mode, size_, sha1, path
            ))

            size -= entry_size

    print("wrote index.")

def extract_archive(size, f):
    if os.path.exists('./destdir'):
        shutil.rmtree('./destdir')

    os.mkdir('./destdir')

    with tempfile.NamedTemporaryFile() as f2:
        while size > 0:
            to_write = max(size, 10*1024*1024)
            f2.write(f.read(to_write))
            size -= to_write

        f2.flush()

        subprocess.run(['tar', '-xf', f2.name, '-C', 'destdir'])


def write_script(size, f, name):
    with open(name, 'wb') as f2:
        f2.write(f.read(size))
        print("wrote %s script." % name)


def main():
    file_ = None

    for p in sorted(os.listdir('.')):
        if p.endswith('.tpm2'):
            file_ = p
            break

    if not file_:
        print("No transport form found in the current directory.")
        exit(1)

    compression = None
    with open(file_, 'rb') as f:
        if f.read(2) == bytes([0x1f, 0x8b]):
            compression = 'gzip'

    if compression == 'gzip':
        with open(file_, 'rb') as f1:
            with open('tmp', 'wb') as f2:
                subprocess.run([GZIP, '-d'], stdin=f1, stdout=f2)

        file_ = 'tmp'

    with open(file_, 'rb') as f:
        version = struct.unpack('<B', f.read(1))[0]
        if version != 1:
            print("Unsupported version: %d." % version)
            exit(1)

        cnt_sections = struct.unpack('<B', f.read(1))[0]
        toc_ = f.read(9 * cnt_sections)

        for i in range(cnt_sections):
            sec = toc_[i*9 : (i+1) * 9]
            type_ = struct.unpack('<B', sec[0:1])[0]
            start = struct.unpack('<I', sec[1:5])[0]
            size = struct.unpack('<I', sec[5:9])[0]

            if type_ == 0x00:
                read_meta_data(size, f)
            elif type_ == 0x01:
                read_index(size, f)
            elif type_ == 0x20:
                write_script(size, f, 'preinst')
            elif type_ == 0x21:
                write_script(size, f, 'configure')
            elif type_ == 0x22:
                write_script(size, f, 'unconfigure')
            elif type_ == 0x23:
                write_script(size, f, 'postrm')
            elif type_ == 0x80:
                extract_archive(size, f)
            else:
                print("Skipping unknown section of type 0x%02x." % type_)
                f.seek(size, 1)

    exit(0)


if __name__ == '__main__':
    main()
