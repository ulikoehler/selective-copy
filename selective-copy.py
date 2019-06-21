#!/usr/bin/env python3
import subprocess
import argparse
import threading
import os
import socket
import shutil
import time
import sys
import asyncio

class TCPListAccumulator(object):
    def __init__(self):
        self.files = []

    async def handle_msg(self, reader, writer):
        while True:
            try:
                data = await reader.readuntil(b'\n')
                self.files.append(data.decode("utf-8").strip())
            except asyncio.IncompleteReadError:
                break
        # Done, cleanup
        writer.close()


class ServerThread(threading.Thread):
    def __init__(self, rport=13485, wport=13486):
        super().__init__()
        self.rport = rport
        self.wport = wport
        self.raccumulator = TCPListAccumulator()
        self.waccumulator = TCPListAccumulator()
        self._startup_finished = False
        self._stop = False
        self._stopped = False

    async def waiter(self):
        while not self._stop:
            await asyncio.sleep(0.1)

    def run(self):
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        # Create servers
        rsrv = asyncio.start_server(self.raccumulator.handle_msg, '127.0.0.1', self.rport, loop=self.loop)
        wsrv = asyncio.start_server(self.waccumulator.handle_msg, '127.0.0.1', self.wport, loop=self.loop)
        self.rserver = self.loop.run_until_complete(rsrv)
        self.wserver = self.loop.run_until_complete(wsrv)
        # Run in this thread until done
        waiter = self.loop.create_task(self.waiter())
        self._startup_finished = True
        self.loop.run_until_complete(waiter)
        self._stopped = True

    def kill(self):
        self._stop = True
        while not self._stopped:
            time.sleep(0.05)
        self.loop.stop()

if __name__ == "__main__":
    # Parse CLI args
    parser = argparse.ArgumentParser()
    parser.add_argument("cmdarg", nargs="+", help="Command and args")
    parser.add_argument("-w", "--workdir", required=True, help="Working directory")
    parser.add_argument("-p", "--prefix", help="The directory to consider for opened files. Defaults to workdir.")
    parser.add_argument("-c", "--copy-to", help="The directory to copy only opened files to.")
    args = parser.parse_args()
    # Start server
    srv = ServerThread(13485, 13486)
    srv.start()
    # Wait until server startup complete
    while not srv._startup_finished:
        time.sleep(0.01)
    # Get path of interceptor library
    libopenlog_path = os.path.join(
        os.path.dirname(os.path.realpath(__file__)), "libopenlog.so")
    # Build environment
    custom_env = {
        'LD_PRELOAD': libopenlog_path,
    }
    if args.prefix:
        prefix = args.prefix
        custom_env['LIBOPENLOG_PREFIX'] = os.path.realpath(args.prefix)
    else:
        prefix = args.workdir
        custom_env['LIBOPENLOG_PREFIX'] = os.path.realpath(args.workdir)
    custom_env.update(os.environ)
    # Run subprocess
    process = subprocess.Popen(args.cmdarg, cwd=args.workdir, env=custom_env)
    stdout, stderr = process.communicate()
    # Stop server
    srv.kill()
    # Process result
    if args.copy_to:
        os.makedirs(args.copy_to, exist_ok=True)
    for abspath in srv.raccumulator.files:
        relpath = os.path.relpath(abspath, start=prefix)
        if args.copy_to: # if we should copy
            dstpath = os.path.join(args.copy_to, relpath)
            # Create dst directory if doesnt exist
            dstdir = os.path.dirname(dstpath)
            os.makedirs(dstdir, exist_ok=True)
            # Copy file (try to conserve metadata)
            shutil.copy2(abspath, dstpath)
            print('Copied {} to {}'.format(relpath, dstpath))
        else: # we wont copy => just print
            print(relpath)

                