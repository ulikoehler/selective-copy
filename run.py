#!/usr/bin/env python3
import subprocess
import argparse
import os

if __name__ == "__main__":
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