#!/usr/bin/env python3
import subprocess
import argparse
import threading
import os
import socket
import time
import sys
import asyncio
from stopit import async_raise

async def handle_msg(reader, writer):
    data = await reader.readline()
    message = data.decode()
    addr = writer.get_extra_info('peername')
    print("Received %r from %r" % (message, addr))
    print("Close the client socket")
    writer.close()


class ServerThread(threading.Thread):
    def __init__(self, port):
        super().__init__()
        self.port = port

    def run(self):
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        srv = asyncio.start_server(handle_msg, '127.0.0.1', self.port, loop=self.loop)
        self.server = self.loop.run_until_complete(srv)
        self.loop.run_forever()

    def kill(self):
        self.server.close()

if __name__ == "__main__":
    srv = ServerThread(13485)
    srv.start()
    parser = argparse.ArgumentParser()
    parser.add_argument("cmdarg", nargs="+", help="Command and args")
    parser.add_argument("-w", "--workdir", required=True, help="Working directory")
    args = parser.parse_args()
    # Get path of interceptor library
    libopenlog_path = os.path.join(
        os.path.dirname(os.path.realpath(__file__)), "libopenlog.so")
    # Build environment
    custom_env = {
        'LD_PRELOAD': libopenlog_path,
        'LIBOPENLOG_RLOGFILE': '/tmp/read.log',
        'LIBOPENLOG_WLOGFILE': '/tmp/write.log'
    }
    custom_env.update(os.environ)
    # Run subprocess
    process = subprocess.Popen(args.cmdarg, cwd=args.workdir, env=custom_env)
    stdout, stderr = process.communicate()
    # Stop server
    #srv.loop.call_exceütion_handler({"message": "", "exception": KeyboardInterrupt()})
    srv.loop.stop()
    async_raise(srv.ident, KeyboardInterrupt)
    time.sleep(0.1)