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
ADCS_SERVICE_PORT = 10

# The CSP service definitions below mirror libcsp's csp_types.h and csp_cmp.h.
class CspServicePort(enum.IntEnum):
    CMP = 0
    PING = 1
    MEMFREE = 3
    REBOOT = 4
    BUF_FREE = 5
    UPTIME = 6


CSP_REBOOT_MAGIC = 0x80078007
CSP_REBOOT_SHUTDOWN_MAGIC = 0xD1E5529A

CSP_CMP_REQUEST = 0x00
CSP_CMP_REPLY = 0xFF


class CspCmpCode(enum.IntEnum):
    IDENT = 1
    CLOCK = 6


CSP_U32_RES = struct.Struct(">I")  # memfree, buf_free, uptime and reboot magic are big endian
CSP_CMP_HEADER = struct.Struct("<BB")  # type, code
CSP_CMP_IDENT = struct.Struct("<20s30s20s12s9s")  # hostname, model, revision, date, time
CSP_CMP_CLOCK = struct.Struct(">II")  # tv_sec, tv_nsec

# The ADCS definitions below mirror include/finch/adcs_service/adcs_service.h and must be kept in sync with it.
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
    sock = await node.connect(dst=OBC_ADDR, port=ADCS_SERVICE_PORT, send_flags=SEND_FLAGS)
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


async def request_u32(node: csp_py.CspNode, port: CspServicePort) -> int | None:
    sock = await node.connect(dst=OBC_ADDR, port=port, send_flags=SEND_FLAGS)
    await sock.send(data=bytearray())
    response = await sock.recv()
    if len(response.data) != CSP_U32_RES.size:
        print(f"Reply from node {OBC_ADDR}: unexpected size {len(response.data)} (expected {CSP_U32_RES.size})")
        return None
    (value,) = CSP_U32_RES.unpack(bytes(response.data))
    return value


async def request_cmp(node: csp_py.CspNode, code: CspCmpCode, payload: struct.Struct, data: bytes) -> tuple | None:
    sock = await node.connect(dst=OBC_ADDR, port=CspServicePort.CMP, send_flags=SEND_FLAGS)
    await sock.send(data=bytearray(CSP_CMP_HEADER.pack(CSP_CMP_REQUEST, code) + data))
    response = await sock.recv()
    expected = CSP_CMP_HEADER.size + payload.size
    if len(response.data) != expected:
        print(f"Reply from node {OBC_ADDR}: unexpected size {len(response.data)} (expected {expected})")
        return None
    cmp_type, cmp_code = CSP_CMP_HEADER.unpack_from(bytes(response.data))
    if cmp_type != CSP_CMP_REPLY or cmp_code != code:
        print(f"Reply from node {OBC_ADDR}: unexpected CMP type=0x{cmp_type:02x} code={cmp_code}")
        return None
    return payload.unpack_from(bytes(response.data), CSP_CMP_HEADER.size)


async def cmd_cmp_ident(node: csp_py.CspNode):
    print(f"Requesting identity from node {OBC_ADDR}...")
    fields = await request_cmp(node, CspCmpCode.IDENT, CSP_CMP_IDENT, bytes(CSP_CMP_IDENT.size))
    if fields is None:
        return
    hostname, model, revision, date, time = (f.split(b"\0", 1)[0].decode(errors="replace") for f in fields)
    print(f"Reply from node {OBC_ADDR}: hostname={hostname} model={model} revision={revision} built={date} {time}")


async def cmd_cmp_clock(node: csp_py.CspNode):
    print(f"Requesting clock from node {OBC_ADDR}...")
    # tv_sec = 0 reads the clock without setting it
    fields = await request_cmp(node, CspCmpCode.CLOCK, CSP_CMP_CLOCK, CSP_CMP_CLOCK.pack(0, 0))
    if fields is None:
        return
    tv_sec, tv_nsec = fields
    print(f"Reply from node {OBC_ADDR}: clock={tv_sec}.{tv_nsec:09d} s")


async def cmd_memfree(node: csp_py.CspNode):
    print(f"Requesting free memory from node {OBC_ADDR}...")
    memfree = await request_u32(node, CspServicePort.MEMFREE)
    if memfree is not None:
        print(f"Reply from node {OBC_ADDR}: free memory={memfree} bytes")


async def cmd_buf_free(node: csp_py.CspNode):
    print(f"Requesting free CSP buffers from node {OBC_ADDR}...")
    buf_free = await request_u32(node, CspServicePort.BUF_FREE)
    if buf_free is not None:
        print(f"Reply from node {OBC_ADDR}: free buffers={buf_free}")


async def cmd_uptime(node: csp_py.CspNode):
    print(f"Requesting uptime from node {OBC_ADDR}...")
    uptime = await request_u32(node, CspServicePort.UPTIME)
    if uptime is not None:
        print(f"Reply from node {OBC_ADDR}: uptime={uptime} s")


async def send_reboot_magic(node: csp_py.CspNode, magic: int):
    # The node does not reply to reboot requests
    sock = await node.connect(dst=OBC_ADDR, port=CspServicePort.REBOOT, send_flags=SEND_FLAGS)
    await sock.send(data=bytearray(CSP_U32_RES.pack(magic)))


async def cmd_reboot(node: csp_py.CspNode):
    print(f"Sending reboot to node {OBC_ADDR}...")
    await send_reboot_magic(node, CSP_REBOOT_MAGIC)
    print("Sent (no reply expected)")


async def cmd_shutdown(node: csp_py.CspNode):
    print(f"Sending shutdown to node {OBC_ADDR}...")
    await send_reboot_magic(node, CSP_REBOOT_SHUTDOWN_MAGIC)
    print("Sent (no reply expected)")


COMMANDS = [
    ("ping", cmd_ping),
    ("ident (cmp)", cmd_cmp_ident),
    ("clock (cmp)", cmd_cmp_clock),
    ("memfree", cmd_memfree),
    ("buf free", cmd_buf_free),
    ("uptime", cmd_uptime),
    ("reboot", cmd_reboot),
    ("shutdown", cmd_shutdown),
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
