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


def init_database():
    conn = sqlite3.connect(DB_FILE)  #
    cursor = conn.cursor()
    cursor.execute('''CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY, password TEXT)''')
    conn.commit()
    conn.close()

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


def handle_client(client_socket: socket.socket, addr):
    print(f"[{SERVER_NAME}] Connected to Java server at {addr}")
    try:
        while True:
            data = b""
            while True:
                chunk = client_socket.recv(1024)
                if not chunk: break
                data += chunk
                if b"\0" in data: break
            
            if not data: break
            
            query = data.decode("utf-8").strip("\0")
            print(f"[{SERVER_NAME}] Executing: {query}")
            
            if query.upper().startswith("SELECT"):
                response = execute_sql_query(query)
            else:
                response = execute_sql_command(query)
            
            # שורה זו חייבת להיות באותו קו עם ה-if שמעליה!
            client_socket.sendall((response + "\0").encode("utf-8"))
            
    except Exception as e:
        print(f"[{SERVER_NAME}] Connection error with {addr}: {e}")
    finally:
        # ה-finally חייב להיות מיושר עם ה-try וה-except!
        client_socket.close()


def start_server(host="127.0.0.1", port=7778):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        server_socket.bind((host, port))
        server_socket.listen(5)
        print(f"[{SERVER_NAME}] Server started on {host}:{port}")
        print(f"[{SERVER_NAME}] Waiting for connections...")

        while True:
            client_socket, addr = server_socket.accept()
            t = threading.Thread(
                target=handle_client,
                args=(client_socket, addr),
                daemon=True
            )
            t.start()

    except KeyboardInterrupt:
        print(f"\n[{SERVER_NAME}] Shutting down server...")
    finally:
        try:
            server_socket.close()
        except Exception:
            pass


if __name__ == "__main__":
    port = 7778
    if len(sys.argv) > 1:
        raw_port = sys.argv[1].strip()
        try:
            port = int(raw_port)
        except ValueError:
            print(f"Invalid port '{raw_port}', falling back to default {port}")

    start_server(port=port)
