# Copyright (c) 2026 The FINCH CubeSat Project Flight Software Contributors
# SPDX-License-Identifier: Apache-2.0

# Send CSP packets to the OBC over SocketCAN using csp_py (pip install cubesat-space-protocol-py).
#
# Usage:
#   python csptool.py [--channel can0]
# then pick a command from the menu.

import argparse
import asyncio
import enum
import struct

import can
import csp_py
from csp_py.interfaces.can import CspCanInterface

OBC_ADDR = 1
OWN_ADDR = 3
ADCS_CMD_PORT = 10

# The ADCS definitions below mirror apps/obc/src/adcs_service.h and must be kept in sync with it.
ADCS_ID_SIZE = 12


class AdcsServiceCmdType(enum.IntEnum):
    GET_ID = 0x01


class AdcsServiceStatus(enum.IntEnum):
    OK = 0x00
    ERR_UNKNOWN_CMD = 0x01
    ERR_ADCS = 0x02


ADCS_SERVICE_CMD = struct.Struct("<B")  # type
ADCS_SERVICE_RES = struct.Struct(f"<BB{ADCS_ID_SIZE}s")  # status, cmd_type, data

# Important: The CRC32 send flag is needed becuause of an issue in libcsp where the check for CRC is hard coded in csp_bind_callback (See https://github.com/libcsp/libcsp/issues/972)
# The issue should be resolved in a few days so the CRC flag should be removed when that happens.
SEND_FLAGS = csp_py.CspPacketFlags.CRC32


async def handle_incoming_frames(iface: CspCanInterface, bus: can.BusABC):
    reader = can.AsyncBufferedReader()
    can.Notifier(bus, [reader], loop=asyncio.get_running_loop())
    while True:
        msg = await reader.get_message()
        if msg.is_extended_id and not msg.is_error_frame:
            await iface.on_can_frame(msg.arbitration_id, bytes(msg.data))


async def cmd_ping(node: csp_py.CspNode):
    size = 100
    print(f"Pinging node {OBC_ADDR} with {size} bytes...")
    rtt = await csp_py.ping(node, dst=OBC_ADDR, size=size, send_flags=SEND_FLAGS)
    print(f"Reply from node {OBC_ADDR}: bytes={size} time={rtt * 1000:.1f} ms")


async def cmd_adcs_get_id(node: csp_py.CspNode):
    print(f"Requesting ADCS ID from node {OBC_ADDR}...")
    sock = await node.connect(dst=OBC_ADDR, port=ADCS_CMD_PORT, send_flags=SEND_FLAGS)
    await sock.send(data=bytearray(ADCS_SERVICE_CMD.pack(AdcsServiceCmdType.GET_ID)))
    response = await sock.recv()
    if len(response.data) != ADCS_SERVICE_RES.size:
        print(f"Reply from node {OBC_ADDR}: unexpected size {len(response.data)} (expected {ADCS_SERVICE_RES.size})")
        return
    status, cmd_type, data = ADCS_SERVICE_RES.unpack(bytes(response.data))
    if status != AdcsServiceStatus.OK:
        try:
            status_name = AdcsServiceStatus(status).name
        except ValueError:
            status_name = "UNKNOWN"
        print(f"Reply from node {OBC_ADDR}: error status=0x{status:02x} ({status_name})")
        return
    print(f"Reply from node {OBC_ADDR}: cmd=0x{cmd_type:02x} id={data.hex(' ')}")


COMMANDS = [
    ("ping", cmd_ping),
    ("adcs get id", cmd_adcs_get_id),
]

REPLY_TIMEOUT_S = 5


async def menu(node: csp_py.CspNode):
    while True:
        print()
        for i, (name, _) in enumerate(COMMANDS, start=1):
            print(f"  {i}) {name}")
        print("  q) exit")

        # Read input on a thread so the router and CAN rx tasks keep running
        choice = (await asyncio.to_thread(input, "> ")).strip().lower()
        if choice in ("q", "quit", "exit"):
            return
        if not choice.isdigit() or not 1 <= int(choice) <= len(COMMANDS):
            print("Invalid choice")
            continue

        _, cmd = COMMANDS[int(choice) - 1]
        try:
            await asyncio.wait_for(cmd(node), timeout=REPLY_TIMEOUT_S)
        except asyncio.TimeoutError:
            print(f"No reply within {REPLY_TIMEOUT_S}s")


async def main(channel: str):
    node = csp_py.CspNode()
    iface = CspCanInterface()
    bus = can.Bus(interface="socketcan", channel=channel, bitrate=100000)

    async def send_can_frame(can_id: int, data: bytes):
        bus.send(can.Message(arbitration_id=can_id, data=data, is_extended_id=True))

    iface.send_can_frame = send_can_frame
    node.router.add_interface(iface, address=OWN_ADDR, netmask_bits=10) # Add CAN interface to CSP node

    router = asyncio.create_task(node.router.arun()) # Set up CSP Router thread
    rx = asyncio.create_task(handle_incoming_frames(iface, bus)) # Callback for incoming frames

    try:
        await menu(node)
    finally:
        rx.cancel()
        router.cancel()
        bus.shutdown()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Send CSP commands to the OBC over CAN")
    parser.add_argument("--channel", default="can0", help="SocketCAN channel (default: can0)")
    args = parser.parse_args()

    try:
        asyncio.run(main(args.channel))
    except (KeyboardInterrupt, EOFError):
        pass
