#!/usr/bin/env python3
import socket
import sys
import threading
import sqlite3

SERVER_NAME = "STOMP_PYTHON_SQL_SERVER"
DB_FILE = "stomp_server.db"

# מפה לניהול הרשמות: { topic: { client_socket: sub_id } }
subscriptions = {}
subscriptions_lock = threading.Lock()

def recv_null_terminated(sock: socket.socket) -> str:
    """קריאת פריים עד לתו ה-NULL כפי שנדרש ב-STOMP"""
    data = b""
    while True:
        try:
            chunk = sock.recv(1024)
            if not chunk:
                return ""
            data += chunk
            if b"\0" in data:
                msg, _ = data.split(b"\0", 1)
                return msg.decode("utf-8", errors="replace")
        except Exception:
            return ""

def init_db():
    conn = sqlite3.connect(DB_FILE)
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

def parse_stomp_frame(frame: str):
    """פירוק פריים STOMP לפקודה, כותרות וגוף"""
    lines = frame.split("\n")
    if not lines or not lines[0]:
        return None, {}, ""
    
    command = lines[0].strip()
    headers = {}
    i = 1
    while i < len(lines) and lines[i].strip():
        if ":" in lines[i]:
            k, v = lines[i].split(":", 1)
            headers[k.strip()] = v.strip()
        i += 1
    
    body = "\n".join(lines[i+1:])
    return command, headers, body

def handle_client(client_socket: socket.socket, addr):
    print(f"[{SERVER_NAME}] Connected to client at {addr}")
    try:
        while True:
            raw_frame = recv_null_terminated(client_socket)
            if not raw_frame:
                break

            command, headers, body = parse_stomp_frame(raw_frame)
            if not command: continue

            print(f"[{SERVER_NAME}] Received {command} from {addr}")

            # --- CONNECT ---
            if command == "CONNECT":
                # בגרסה פשוטה זו אנו מאשרים תמיד, ניתן להוסיף בדיקת משתמש ב-DB
                response = "CONNECTED\nversion:1.2\n\n"
                client_socket.sendall((response + "\0").encode("utf-8"))

            # --- SUBSCRIBE ---
            elif command == "SUBSCRIBE":
                dest = headers.get("destination")
                sub_id = headers.get("id")
                if dest and sub_id:
                    with subscriptions_lock:
                        if dest not in subscriptions:
                            subscriptions[dest] = {}
                        subscriptions[dest][client_socket] = sub_id
                
                # שליחת RECEIPT אם התבקש (הלקוח שלך מצפה לזה ב-Join)
                if "receipt" in headers:
                    receipt_id = headers["receipt"]
                    res = f"RECEIPT\nreceipt-id:{receipt_id}\n\n"
                    client_socket.sendall((res + "\0").encode("utf-8"))

            # --- SEND ---
            elif command == "SEND":
                dest = headers.get("destination")
                if dest:
                    # בניית פריים MESSAGE להפצה
                    message_frame = f"MESSAGE\nsubscription:0\ndestination:{dest}\n\n{body}"
                    with subscriptions_lock:
                        if dest in subscriptions:
                            for sock in list(subscriptions[dest].keys()):
                                try:
                                    # עדכון ה-subscription ID הספציפי לכל לקוח
                                    sub_id = subscriptions[dest][sock]
                                    personalized_frame = message_frame.replace("subscription:0", f"subscription:{sub_id}")
                                    sock.sendall((personalized_frame + "\0").encode("utf-8"))
                                except:
                                    del subscriptions[dest][sock]

            # --- DISCONNECT ---
            elif command == "DISCONNECT":
                if "receipt" in headers:
                    receipt_id = headers["receipt"]
                    res = f"RECEIPT\nreceipt-id:{receipt_id}\n\n"
                    client_socket.sendall((res + "\0").encode("utf-8"))
                break

            # --- SQL FALLBACK ---
            # אם הפריים הוא פקודת SQL ישירה (כמו ב-report של הלקוח שלך)
            elif "INSERT" in command.upper() or "UPDATE" in command.upper():
                result = execute_sql_command(raw_frame)
                client_socket.sendall((result + "\0").encode("utf-8"))

    except Exception as e:
        print(f"[{SERVER_NAME}] Error: {e}")
    finally:
        client_socket.close()
        with subscriptions_lock:
            for dest in subscriptions:
                if client_socket in subscriptions[dest]:
                    del subscriptions[dest][client_socket]
        print(f"[{SERVER_NAME}] Disconnected {addr}")

def start_server(port=7778):
    init_db()
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('0.0.0.0', port))
    server.listen(10)
    print(f"[{SERVER_NAME}] Listening on port {port}...")
    while True:
        client, addr = server.accept()
        threading.Thread(target=handle_client, args=(client, addr), daemon=True).start()


if __name__ == "__main__":
    port = 7778
    if len(sys.argv) > 1:
        raw_port = sys.argv[1].strip()
        try:
            port = int(raw_port)
        except ValueError:
            print(f"Invalid port '{raw_port}', falling back to default {port}")

    start_server(port=port)
    