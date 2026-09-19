#!/usr/bin/env python3
"""Run separate local sender/receiver processes; never imply remote qualification."""
import argparse
import json
from pathlib import Path
import socket
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=Path('build/udp-release'))
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--sample-rate', type=int, default=1_000_000)
    parser.add_argument('--streams', type=int, default=4)
    parser.add_argument('--warmup-seconds', type=float, default=1)
    parser.add_argument('--duration-seconds', type=float, default=5)
    args = parser.parse_args()
    if not 1 <= args.streams <= 4 or not 1 <= args.sample_rate <= 100_000_000:
        parser.error('unsupported stream count or rate')
    if not 0 <= args.warmup_seconds <= 60 or not 0 < args.duration_seconds <= 3600:
        parser.error('unsupported duration')
    root = args.output_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    receiver_dir, sender_dir = root / 'receiver', root / 'sender'
    receiver_dir.mkdir(); sender_dir.mkdir()
    executable = lambda name: str((args.build_dir / 'bench' / name).resolve())
    children, reservations, logs = [], [], []
    try:
        ports = []
        for _ in range(3):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind(('127.0.0.1', 0)); reservations.append(sock)
            ports.append(sock.getsockname()[1])
        peer_ports = [item for lane, port in zip(('data', 'control', 'cancel'), ports)
                      for item in (f'--sender-{lane}-port', str(port))]
        common = ['--streams', str(args.streams), '--sample-rate', str(args.sample_rate)]
        receiver = [executable('vita_receiver_benchmark'), '--bind-ip', '127.0.0.1',
                    '--data-port', '0', '--control-port', '0', '--cancel-port', '0',
                    '--sender-ip', '127.0.0.1', *peer_ports, *common,
                    '--warmup-seconds', str(args.warmup_seconds),
                    '--duration-seconds', str(args.duration_seconds),
                    '--output-dir', str(receiver_dir)]
        def start(command, directory):
            out = (directory / 'run.log').open('w')
            err = (directory / 'process.log').open('w')
            logs.extend((out, err))
            child = subprocess.Popen(command, stdout=out, stderr=err)
            children.append(child)
            return child
        receive_process = start(receiver, receiver_dir)
        ready_path = receiver_dir / 'ready.json'
        deadline = time.monotonic() + 15
        ready = None
        while time.monotonic() < deadline:
            if receive_process.poll() is not None:
                raise RuntimeError('receiver exited before readiness; inspect process.log')
            if ready_path.exists():
                try:
                    ready = json.loads(ready_path.read_text())
                    break
                except json.JSONDecodeError:
                    pass
            time.sleep(0.01)
        if ready is None:
            raise RuntimeError('receiver readiness timed out')
        destinations = [item for lane in ('data', 'control', 'cancel')
                        for item in (f'--receiver-{lane}-port', str(ready[f'{lane}_port']))]
        own_ports = [item for lane, port in zip(('data', 'control', 'cancel'), ports)
                     for item in (f'--{lane}-port', str(port))]
        sender = [executable('vita_replay_sender'), '--bind-ip', '127.0.0.1',
                  '--receiver-ip', '127.0.0.1', *destinations, *own_ports, *common,
                  '--duration-seconds', str(args.warmup_seconds + args.duration_seconds + 0.1),
                  '--output-dir', str(sender_dir)]
        provenance = {'scope': 'local replay; separate processes on the same host',
                      'packet_pairs': 256, 'streams': args.streams,
                      'sample_rate_per_stream': args.sample_rate,
                      'receiver_command': receiver, 'sender_command': sender,
                      'receiver_ready': ready, 'cross_host_latency_claimed': False}
        (root / 'invocation.json').write_text(json.dumps(provenance, indent=2) + '\n')
        for sock in reservations:
            sock.close()
        reservations.clear()
        send_process = start(sender, sender_dir)
        timeout = args.warmup_seconds + args.duration_seconds + 20
        sender_exit = send_process.wait(timeout=timeout)
        receiver_exit = receive_process.wait(timeout=timeout)
        provenance.update(sender_exit=sender_exit, receiver_exit=receiver_exit)
        (root / 'invocation.json').write_text(json.dumps(provenance, indent=2) + '\n')
        print(json.dumps({'output': str(root), 'sender_exit': sender_exit,
                          'receiver_exit': receiver_exit}))
        return 0 if sender_exit == receiver_exit == 0 else 1
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f'FAILED: {error}')
        (root / 'orchestration-error.txt').write_text(str(error) + '\n')
        return 1
    finally:
        for child in children:
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    child.kill(); child.wait()
        for sock in reservations:
            sock.close()
        for file in logs:
            file.close()


if __name__ == '__main__':
    raise SystemExit(main())
