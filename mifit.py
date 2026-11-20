import json
import os
import socket
import struct

__doc__ = """MIFit interaction module

Defines functions for interfacing with a MIFit session.
The environment must have MIFIT_SOCKET_ID for an
associated MIFit session. This will be the case for
scripts run from the MIFit Jobs menu.

Protocol: a Unix domain socket (MIFIT_SOCKET_ID is its path); each message is
a 4-byte big-endian length followed by UTF-8 JSON.
Request: {"cmd": name, "args": [...]}
Reply:   {"ok": true, "result": ...} or {"ok": false, "error": message}
"""

socketId = None
mifitDir = None
shelxDir = None

if 'MIFIT_SOCKET_ID' in os.environ.keys():
    socketId = os.environ['MIFIT_SOCKET_ID']

if 'MIFIT_DIR' in os.environ.keys():
    mifitDir = os.environ['MIFIT_DIR']

if 'SHELX_DIR' in os.environ.keys():
    shelxDir = os.environ['SHELX_DIR']

_TIMEOUT = 30.0


def _recv_exact(sock, count):
    chunks = []
    while count > 0:
        chunk = sock.recv(count)
        if not chunk:
            raise ConnectionError('MIFit closed the connection')
        chunks.append(chunk)
        count -= len(chunk)
    return b''.join(chunks)


def call(cmd, *args):
    """Sends a command to the associated MIFit session and returns its result.
Raises RuntimeError if MIFit reports an error."""
    if not socketId:
        raise RuntimeError('MIFIT_SOCKET_ID is not set; not running from a MIFit session')
    payload = json.dumps({'cmd': cmd, 'args': list(args)}).encode('utf-8')
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.settimeout(_TIMEOUT)
    try:
        sock.connect(socketId)
        sock.sendall(struct.pack('>I', len(payload)) + payload)
        length = struct.unpack('>I', _recv_exact(sock, 4))[0]
        reply = json.loads(_recv_exact(sock, length).decode('utf-8'))
    finally:
        sock.close()
    if not reply.get('ok'):
        raise RuntimeError(reply.get('error', 'unknown MIFit error'))
    return reply.get('result')


def version():
    """Returns the version of MIFit"""
    return str(call('version'))

def directory():
    """Returns the installation directory for MIFit"""
    return str(call('directory'))

def scriptPort():
    """Returns the identifier for the local socket used for interaction with MIFit"""
    global socketId
    return socketId

def setScriptPort(id):
    """Sets the identifier for the local socket used for interaction with MIFit.
Usually this will be obtained automatically from the MIFIT_SOCKET_ID environment variable."""
    global socketId
    socketId = id

def setJobWorkDir(dir):
    """Sets the working directory for the current job (when the script is run from MIFit)"""
    return call('setJobWorkDir', os.environ['MIFIT_JOB_ID'], dir)

def writeCurrentModel(file):
    """Causes MIFit to output the current model to the given file"""
    return call('writeCurrentModel', file)

def dictionaryResidueList():
    """Returns the dictionary residue list from MIFit"""
    result = call('dictionaryResidueList')
    if result:
        return list(result)
    return None

def spacegroupList():
    """Returns the spacegroup list from MIFit"""
    result = call('spacegroupList')
    if result:
        return list(result)
    return None

def addJob(menuName, jobName, executable, arguments, workingDirectory):
    """Adds a new custom job to the MIFit Job menu"""
    call('addJob', menuName, jobName, executable, list(arguments), workingDirectory)
