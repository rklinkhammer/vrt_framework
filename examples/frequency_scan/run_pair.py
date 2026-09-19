#!/usr/bin/env python3
"""Bounded localhost process smoke. No protocol generation or parsing here."""
import argparse
import pathlib
import signal
import socket
import subprocess
import tempfile
import time


def reserve_pair():
    for base in range(43000, 60000, 11):
        sockets = []
        try:
            for port in (*range(base, base + 3), *range(base + 5, base + 8)):
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sockets.append(sock)
                sock.bind(("127.0.0.1", port))
            return base, base + 5, sockets
        except OSError:
            for sock in sockets:
                sock.close()
    raise RuntimeError("no free loopback lane pair")


def stop(process):
    if process.poll() is None:
        process.send_signal(signal.SIGINT)
        try:
            process.wait(timeout=4)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=2)
            raise RuntimeError("process failed bounded graceful shutdown")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--controller", required=True)
    parser.add_argument("--controllee")
    parser.add_argument("--timeout-only", action="store_true")
    args = parser.parse_args()
    client, server, reservations = reserve_pair()
    if args.timeout_only:
        # Keep the remote ports bound but silent; the Controller must progress its
        # deadline while idle, without depending on ICMP errors or a responding peer.
        for sock in reservations[:3]:
            sock.close()
        try:
            began = time.monotonic()
            result = subprocess.run([args.controller, "--local-base-port", str(client), "--peer-base-port", str(server), "--timeout-ms", "50"], text=True, capture_output=True, timeout=4)
            elapsed = time.monotonic() - began
            print(result.stdout, end="")
            print(result.stderr, end="")
            if result.returncode != 1 or elapsed < .045 or "scan failed" not in result.stderr or "summary confirmed=0" not in result.stdout:
                raise RuntimeError("idle deadline did not produce an explicit unconfirmed failure")
            # The reserved cancellation socket must remain empty: timeout is not cancel.
            reservations[5].setblocking(False)
            try:
                reservations[5].recv(65535)
            except BlockingIOError:
                pass
            else:
                raise RuntimeError("timeout unexpectedly sent a cancellation datagram")
            return
        finally:
            for sock in reservations:
                sock.close()
    if not args.controllee:
        parser.error("--controllee is required unless --timeout-only")
    processes = []
    with tempfile.TemporaryDirectory(prefix="vita-p16-") as directory:
        directory = pathlib.Path(directory)
        server_path, client_path = directory / "controllee.log", directory / "controller.log"
        try:
            # Reservations minimize accidental collisions; close before child bind.
            # A racing third party can still win a port; startup then fails explicitly.
            for sock in reservations[3:]:
                sock.close()
            with server_path.open("w") as server_log, client_path.open("w") as client_log:
                controllee = subprocess.Popen([args.controllee, "--local-base-port", str(server), "--peer-base-port", str(client), "--duration-ms", "10000"], stdout=server_log, stderr=subprocess.STDOUT)
                processes.append(controllee)
                deadline = time.monotonic() + 5
                while "ready role=controllee" not in server_path.read_text():
                    if controllee.poll() is not None or time.monotonic() >= deadline:
                        raise RuntimeError("Controllee did not become ready")
                    time.sleep(0.01)
                for sock in reservations[:3]:
                    sock.close()
                controller = subprocess.Popen([args.controller, "--local-base-port", str(client), "--peer-base-port", str(server), "--dwell-ms", "2", "--timeout-ms", "2000"], stdout=client_log, stderr=subprocess.STDOUT)
                processes.append(controller)
                controller.wait(timeout=8)
                # Server may still be sending. Request its owner-thread drain promptly.
                stop(controllee)
                client_text, server_text = client_path.read_text(), server_path.read_text()
                print(client_text, end="")
                print(server_text, end="")
                if controller.returncode or controllee.returncode:
                    raise RuntimeError("endpoint failure (including transport or shutdown errors)")
                if "summary confirmed=9" not in client_text or "backend_writes=0" not in client_text:
                    raise RuntimeError("Controller did not confirm all points without local device effects")
                if "backend_writes=9" not in server_text or "shutdown=5" not in server_text:
                    raise RuntimeError("Controllee effect/drain evidence missing")
        finally:
            cleanup_error = None
            for process in reversed(processes):
                try:
                    stop(process)
                except RuntimeError as exc:
                    cleanup_error = exc
            for sock in reservations:
                sock.close()
            if cleanup_error:
                raise cleanup_error


if __name__ == "__main__":
    main()
