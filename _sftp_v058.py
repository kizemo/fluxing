#!/usr/bin/env python3
"""v0.19.0.58 aiec staging SFTP 上传 (Phase K6)."""
import os
import sys
import hashlib
import paramiko

HOST = "47.120.26.175"
PORT = 22
USER = "kizemo"
PASSWORD = "REDACTED-PASSWORD-PLEASE-ROTATE"
REMOTE_BASE = "/var/www/wordpress/pinyin"

LOCAL_BASE = r"E:\办公文件\L1网站\pinyin"

FILES = [
    ("fluxing-0.19.0.58-installer.exe", "312053473F9D41FCA65EF659F2185B9A"),
    ("version.json", None),
    ("appcast.xml", None),
    ("release-notes.html", None),
]


def md5_file(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def main():
    # 1. 本地 md5 预检
    print("=== [1/3] 本地 md5 预检 ===")
    for name, expected in FILES:
        local_path = os.path.join(LOCAL_BASE, name)
        if not os.path.exists(local_path):
            print(f"  [ERR] {name}: 本地不存在")
            sys.exit(1)
        actual = md5_file(local_path)
        if expected and actual.upper() != expected.upper():
            print(f"  [ERR] {name}: 本地 md5 {actual} ≠ expected {expected}")
            sys.exit(1)
        print(f"  OK   {name}: {actual}")

    # 2. SFTP 上传
    print("\n=== [2/3] SFTP 上传 ===")
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        client.connect(HOST, PORT, USER, PASSWORD, timeout=30)
    except Exception as e:
        print(f"  [ERR] SFTP 连接失败: {e}")
        sys.exit(1)
    sftp = client.open_sftp()

    for name, _ in FILES:
        local_path = os.path.join(LOCAL_BASE, name)
        remote_path = f"{REMOTE_BASE}/{name}"
        print(f"  上传 {name}...", end=" ")
        try:
            sftp.put(local_path, remote_path)
            print("OK")
        except Exception as e:
            print(f"FAIL: {e}")
            sftp.close()
            client.close()
            sys.exit(1)

    sftp.close()
    client.close()

    # 3. 服务器端 md5 验证
    print("\n=== [3/3] 服务器端 md5 验证 ===")
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, PORT, USER, PASSWORD, timeout=30)

    for name, expected in FILES:
        # 服务器端用 md5sum 命令 (Linux)
        stdin, stdout, stderr = client.exec_command(f"md5sum '{REMOTE_BASE}/{name}'")
        line = stdout.readline().strip()
        if not line:
            print(f"  [ERR] {name}: md5sum 输出为空")
            continue
        server_md5 = line.split()[0].upper()
        local_md5 = md5_file(os.path.join(LOCAL_BASE, name)).upper()
        match = "[OK]" if server_md5 == local_md5 else "[MISMATCH]"
        print(f"  {match} {name}: server={server_md5} local={local_md5}")

    client.close()
    print("\n[DONE] v0.19.0.58 SFTP 上传完成")


if __name__ == "__main__":
    main()