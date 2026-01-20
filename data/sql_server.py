#!/usr/bin/env python3
"""
Basic Python Server for STOMP Assignment – Stage 3.3

IMPORTANT:
DO NOT CHANGE the server name or the basic protocol.
Students should EXTEND this server by implementing
the methods below.
"""

import socket
import sys
import threading
import sqlite3


SERVER_NAME = "STOMP_PYTHON_SQL_SERVER"  # DO NOT CHANGE!
DB_FILE = "stomp_server.db"              # DO NOT CHANGE!




def recv_null_terminated(sock: socket.socket) -> str:
    data = b""
    while True:
        chunk = sock.recv(1024)
        if not chunk:
            return ""
        data += chunk
        if b"\0" in data:
            msg, _ = data.split(b"\0", 1)
            return msg.decode("utf-8", errors="replace")


def init_db():
    # כל השורות כאן חייבות להיות עם הזחה של 4 רווחים ימינה
    conn = sqlite3.connect('stomp_server.db')
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS users (
            username TEXT PRIMARY KEY,
            password TEXT NOT NULL
        )
    ''')
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user TEXT,
            team TEXT,
            eventType TEXT,
            time INTEGER,
            beforeHalftime INTEGER,
            description TEXT
        )
    ''')
    conn.commit()
    return conn


def execute_sql_command(sql_command: str) -> str:
    try:
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute(sql_command)
        conn.commit()
        conn.close()
        return "done"
    except Exception as e:
        return f"error: {str(e)}"


def execute_sql_query(sql_query: str) -> str:
    try:
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()
        cursor.execute(sql_query)
        result = cursor.fetchone()
        conn.close()
        if result:
            return str(result[0])
        return "not_found"
    except Exception as e:
        return f"error: {str(e)}"

def parse_stomp_frame(frame: str):
    lines = frame.split("\n")
    command = lines[0]

    headers = {}
    body = ""

    i = 1
    while i < len(lines) and lines[i]:
        if ":" in lines[i]:
            k, v = lines[i].split(":", 1)
            headers[k] = v
        i += 1

    body = "\n".join(lines[i+1:])
    return command, headers, body


def handle_client(client_socket: socket.socket, addr):
    print(f"[{SERVER_NAME}] Connected to client at {addr}")

    try:
        while True:
            frame = recv_null_terminated(client_socket)
            if not frame:
                break

            print(f"[{SERVER_NAME}] Received frame:\n{frame}")

            command, headers, body = parse_stomp_frame(frame)

            # CONNECT
            if command == "CONNECT":
                response = "CONNECTED\nversion:1.2\n\n"
                client_socket.sendall((response + "\0").encode("utf-8"))

            # SUBSCRIBE
            elif command == "SUBSCRIBE":
                dest = headers.get("destination")
                if dest:
                    with subscriptions_lock:
                        subscriptions.setdefault(dest, []).append(client_socket)

            # SEND
            elif command == "SEND":
                dest = headers.get("destination")
                if dest:
                    message = (
                        "MESSAGE\n"
                        f"destination:{dest}\n\n"
                        f"{body}"
                    )
                    with subscriptions_lock:
                        for sock in subscriptions.get(dest, []):
                            try:
                                sock.sendall((message + "\0").encode("utf-8"))
                            except:
                                pass

            # DISCONNECT
            elif command == "DISCONNECT":
                break

            # fallback (SQL / garbage)
            else:
                result = execute_sql_command(frame)
                client_socket.sendall((result + "\0").encode("utf-8"))

    except Exception as e:
        print(f"[{SERVER_NAME}] Error with {addr}: {e}")

    finally:
        print(f"[{SERVER_NAME}] Disconnected from {addr}")
        with subscriptions_lock:
            for subs in subscriptions.values():
                if client_socket in subs:
                    subs.remove(client_socket)
        client_socket.close()


def start_server(port=7778):
    init_db()
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('127.0.0.1', port))
    server_socket.listen(5)

    print(f"[{SERVER_NAME}] Server is running on 127.0.0.1:{port}")

    while True:
        client_sock, addr = server_socket.accept()
        threading.Thread(
            target=handle_client,
            args=(client_sock, addr),
            daemon=True
        ).start()


if __name__ == "__main__":
    port = 7778
    if len(sys.argv) > 1:
        raw_port = sys.argv[1].strip()
        try:
            port = int(raw_port)
        except ValueError:
            print(f"Invalid port '{raw_port}', falling back to default {port}")

    start_server(port=port)
    