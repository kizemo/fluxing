"""test_sftp_sync.py — regression tests for sftp_sync.py (constitution R6, P2).

Run: python -m unittest test_sftp_sync.py
Or:  python test_sftp_sync.py
"""
import io
import json
import os
import shutil
import sys
import tempfile
import unittest
from unittest.mock import MagicMock, patch

# Import the module under test
_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(os.path.dirname(_HERE))
sys.path.insert(0, _ROOT)

import paramiko  # noqa: E402
import sftp_sync  # noqa: E402


class TestVersionRegex(unittest.TestCase):
    def test_valid_versions(self):
        for v in ["0.19.0.58", "0.19.0.59", "1.0.0.0", "10.20.30.40"]:
            self.assertTrue(sftp_sync.VERSION_RE.match(v), v)

    def test_invalid_versions(self):
        for v in ["0.19.0", "v0.19.0.58", "0.19.0.58-beta", "abc", "",
                  "0.19.0.58 ", "0.19.0.58\n", "0.19.0.58."]:
            self.assertFalse(sftp_sync.VERSION_RE.match(v), repr(v))


class TestValidateMd5(unittest.TestCase):
    def test_uppercase_passthrough(self):
        self.assertEqual(sftp_sync.validate_md5("A" * 32, "k"),
                         "A" * 32)

    def test_lowercase_canonicalized(self):
        self.assertEqual(sftp_sync.validate_md5("abcdef01" + "0" * 24, "k"),
                         "ABCDEF01" + "0" * 24)

    def test_empty_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_md5("", "k")

    def test_none_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_md5(None, "k")

    def test_short_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_md5("ABCDEF", "k")

    def test_long_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_md5("A" * 33, "k")

    def test_non_hex_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_md5("G" * 32, "k")


class TestMd5File(unittest.TestCase):
    def test_known_content(self):
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"hello\n")
            path = f.name
        try:
            # md5("hello\n") = b1946ac92492d2347c6235b4d2611184
            self.assertEqual(sftp_sync.md5_file(path),
                             "B1946AC92492D2347C6235B4D2611184")
        finally:
            os.unlink(path)


class TestLoadJson(unittest.TestCase):
    def test_missing_file_aborts(self):
        with self.assertRaises(SystemExit):
            sftp_sync.load_json("/nonexistent/path.json", "Test")

    def test_parse_error_aborts(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json",
                                         delete=False) as f:
            f.write("not valid json {")
            path = f.name
        try:
            with self.assertRaises(SystemExit):
                sftp_sync.load_json(path, "Test")
        finally:
            os.unlink(path)

    def test_valid(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json",
                                         delete=False) as f:
            json.dump({"key": "value"}, f)
            path = f.name
        try:
            self.assertEqual(sftp_sync.load_json(path, "Test"),
                             {"key": "value"})
        finally:
            os.unlink(path)


class TestLoadSftpCredentials(unittest.TestCase):
    def _write(self, data):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json",
                                         delete=False) as f:
            json.dump(data, f)
            return f.name

    def test_valid(self):
        path = self._write({
            "host": "x.com", "port": 22, "username": "u",
            "password": "p", "remotePath": "/r", "protocol": "sftp"
        })
        try:
            cfg = sftp_sync.load_sftp_credentials(path)
            self.assertEqual(cfg["host"], "x.com")
        finally:
            os.unlink(path)

    def test_missing_field_aborts(self):
        path = self._write({"host": "x.com", "port": 22})
        try:
            with self.assertRaises(SystemExit):
                sftp_sync.load_sftp_credentials(path)
        finally:
            os.unlink(path)

    def test_placeholder_password_aborts(self):
        path = self._write({
            "host": "x.com", "port": 22, "username": "u",
            "password": "PLACEHOLDER", "remotePath": "/r",
            "protocol": "sftp"
        })
        try:
            with self.assertRaises(SystemExit):
                sftp_sync.load_sftp_credentials(path)
        finally:
            os.unlink(path)

    def test_empty_password_aborts(self):
        path = self._write({
            "host": "x.com", "port": 22, "username": "u",
            "password": "", "remotePath": "/r", "protocol": "sftp"
        })
        try:
            with self.assertRaises(SystemExit):
                sftp_sync.load_sftp_credentials(path)
        finally:
            os.unlink(path)


class TestParseExpectedMd5(unittest.TestCase):
    def _write_ps1(self, content):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".ps1",
                                         delete=False) as f:
            f.write(content)
            return f.name

    def test_valid(self):
        path = self._write_ps1(
            "$expectedInstallerMd5 = 'ABCDEF0123456789ABCDEF0123456789'\n"
            "$expectedMd5 = '0123456789ABCDEF0123456789ABCDEF'\n"
        )
        try:
            result = sftp_sync.parse_expected_md5(
                path, ["expectedInstallerMd5", "expectedMd5"])
            self.assertEqual(result["expectedInstallerMd5"],
                             "ABCDEF0123456789ABCDEF0123456789")
            self.assertEqual(result["expectedMd5"],
                             "0123456789ABCDEF0123456789ABCDEF")
        finally:
            os.unlink(path)

    def test_missing_key_returns_none(self):
        path = self._write_ps1(
            "$expectedMd5 = 'ABCDEF0123456789ABCDEF0123456789'\n")
        try:
            result = sftp_sync.parse_expected_md5(
                path, ["expectedInstallerMd5"])
            self.assertIsNone(result["expectedInstallerMd5"])
        finally:
            os.unlink(path)

    def test_malformed_value_fails_fast(self):
        # 3 hex chars — regex matches but validate_md5 rejects (not 32 chars)
        path = self._write_ps1("$expectedInstallerMd5 = 'ABC'\n")
        try:
            with self.assertRaises(SystemExit):
                sftp_sync.parse_expected_md5(
                    path, ["expectedInstallerMd5"])
        finally:
            os.unlink(path)

    def test_missing_source_file_aborts(self):
        with self.assertRaises(SystemExit):
            sftp_sync.parse_expected_md5("/nonexistent.ps1", ["k"])


class TestRenderFiles(unittest.TestCase):
    """BLOCKER 1 regression: configured expectedKey absent in ps1 = fail-fast."""

    def test_version_substitution(self):
        files = [{"name": "fluxing-{version}-installer.exe",
                  "expectedKey": "k"}]
        rendered = sftp_sync.render_files(
            files, "0.19.0.59",
            {"k": "ABCDEF0123456789ABCDEF0123456789"})
        self.assertEqual(rendered[0]["name"],
                         "fluxing-0.19.0.59-installer.exe")
        self.assertEqual(rendered[0]["expected"],
                         "ABCDEF0123456789ABCDEF0123456789")

    def test_fail_fast_on_missing_expected_key(self):
        files = [{"name": "fluxing-{version}-installer.exe",
                  "expectedKey": "k"}]
        # Previously this silently returned expected=None (L30/L107 trap).
        with self.assertRaises(SystemExit):
            sftp_sync.render_files(files, "0.19.0.59", {"k": None})

    def test_null_expected_key_passes(self):
        files = [{"name": "version.json", "expectedKey": None}]
        rendered = sftp_sync.render_files(files, "0.19.0.59", {})
        self.assertEqual(rendered[0]["name"], "version.json")
        self.assertIsNone(rendered[0]["expected"])


class TestPrecheckLocal(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def _write(self, name, content):
        path = os.path.join(self.tmpdir, name)
        with open(path, "wb") as f:
            f.write(content)
        return path

    def test_ok(self):
        self._write("a.txt", b"hello")
        md5 = sftp_sync.md5_file(os.path.join(self.tmpdir, "a.txt"))
        files = [{"name": "a.txt", "expected": md5}]
        out = sftp_sync.precheck_local(files, self.tmpdir)
        self.assertEqual(out["a.txt"], md5)

    def test_mismatch_aborts(self):
        self._write("a.txt", b"hello")
        files = [{"name": "a.txt",
                  "expected": "0123456789ABCDEF0123456789ABCDEF"}]
        with self.assertRaises(SystemExit):
            sftp_sync.precheck_local(files, self.tmpdir)

    def test_missing_local_aborts(self):
        files = [{"name": "missing.txt", "expected": None}]
        with self.assertRaises(SystemExit):
            sftp_sync.precheck_local(files, self.tmpdir)


class TestRemoteExists(unittest.TestCase):
    def test_exists(self):
        sftp = MagicMock()
        sftp.stat.return_value = MagicMock()
        self.assertTrue(sftp_sync.remote_exists(sftp, "/r/x"))

    def test_not_exists(self):
        sftp = MagicMock()
        sftp.stat.side_effect = IOError("not found")
        self.assertFalse(sftp_sync.remote_exists(sftp, "/r/x"))


class TestUploadAtomic(unittest.TestCase):
    """BLOCKER 3 regression: atomic upload with rollback on any failure."""

    SUFFIX = sftp_sync.STAGE_SUFFIX   # for assertions
    PRESHIP = sftp_sync.PRESHIP_SUFFIX

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.local_base = self.tmpdir
        self.remote_base = "/remote"
        self.version = "0.19.0.59"
        # Use real run-id so the suffix matches what upload_atomic produces.
        self.run_id = sftp_sync.make_run_id(self.version)
        self.stage_suffix = f"{sftp_sync.STAGE_SUFFIX}-{self.run_id}"
        self.preship_suffix = f"{sftp_sync.PRESHIP_SUFFIX}-{self.run_id}"
        # Write local files first so we can compute md5.
        self.filenames = ["a.txt", "b.txt"]
        for name in self.filenames:
            with open(os.path.join(self.local_base, name), "wb") as fh:
                fh.write(b"data-" + name.encode())
        self.local_md5s = {
            name: sftp_sync.md5_file(
                os.path.join(self.local_base, name))
            for name in self.filenames
        }
        # expected matches local md5 (so staged verify passes for happy path).
        # Tests that want a mismatch override per-test.
        self.files = [
            {"name": "a.txt", "expected": self.local_md5s["a.txt"]},
            {"name": "b.txt", "expected": None},
        ]

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def _mock_sftp_client(self, server_md5s):
        """Mock sftp + ssh client. server_md5s[remote_path] -> md5 hex.
        Returns str (matches paramiko.ChannelFile.readline in Python 3)."""
        sftp = MagicMock()
        sftp_client = MagicMock()

        def exec(cmd):
            stdout = MagicMock()
            cmd_str = cmd if isinstance(cmd, str) else cmd.decode()
            matched_path = None
            matched_md5 = None
            for path, md5 in server_md5s.items():
                if path in cmd_str:
                    matched_path = path
                    matched_md5 = md5
                    break
            stdout.readline.return_value = (
                f"{matched_md5}  {matched_path}\n"
                if matched_md5 else ""
            )
            return MagicMock(), stdout, MagicMock()
        sftp_client.exec_command = exec
        return sftp, sftp_client

    def _stat_side_effect(self, existing_paths):
        def side_effect(path):
            if path in existing_paths:
                return MagicMock()
            raise IOError("not found")
        return side_effect

    def test_happy_path(self):
        sftp, sftp_client = self._mock_sftp_client({
            f"{self.remote_base}/a.txt{self.stage_suffix}":
                self.local_md5s["a.txt"],
            f"{self.remote_base}/b.txt{self.stage_suffix}":
                self.local_md5s["b.txt"],
            f"{self.remote_base}/a.txt": self.local_md5s["a.txt"],
            f"{self.remote_base}/b.txt": self.local_md5s["b.txt"],
        })
        sftp.stat.side_effect = self._stat_side_effect(
            {f"{self.remote_base}/a.txt"})
        ok = sftp_sync.upload_atomic(
            sftp, sftp_client, self.local_base, self.remote_base,
            self.files, self.local_md5s, self.version)
        self.assertTrue(ok)
        # a.txt was snapshotted
        sftp.rename.assert_any_call(
            f"{self.remote_base}/a.txt",
            f"{self.remote_base}/a.txt{self.preship_suffix}")
        # staged renamed to final
        sftp.rename.assert_any_call(
            f"{self.remote_base}/a.txt{self.stage_suffix}",
            f"{self.remote_base}/a.txt")
        sftp.rename.assert_any_call(
            f"{self.remote_base}/b.txt{self.stage_suffix}",
            f"{self.remote_base}/b.txt")
        # preship cleanup at end
        sftp.remove.assert_any_call(
            f"{self.remote_base}/a.txt{self.preship_suffix}")

    def test_staged_md5_mismatch_aborts(self):
        # Override expected to force mismatch
        files = [
            {"name": "a.txt", "expected": "0" * 32},  # wrong expected
            {"name": "b.txt", "expected": None},
        ]
        sftp, sftp_client = self._mock_sftp_client({
            f"{self.remote_base}/a.txt{self.stage_suffix}":
                self.local_md5s["a.txt"],
            f"{self.remote_base}/b.txt{self.stage_suffix}":
                self.local_md5s["b.txt"],
        })
        sftp.stat.side_effect = IOError("not found")
        ok = sftp_sync.upload_atomic(
            sftp, sftp_client, self.local_base, self.remote_base,
            files, self.local_md5s, self.version)
        self.assertFalse(ok)
        # No staged → final renames should have happened
        for c in sftp.rename.call_args_list:
            if len(c.args) >= 2:
                dst = c.args[1]
                self.assertFalse(
                    dst in (f"{self.remote_base}/a.txt",
                            f"{self.remote_base}/b.txt"),
                    f"Unexpected rename to final: {c}")

    def test_rename_failure_aborts(self):
        sftp, sftp_client = self._mock_sftp_client({
            f"{self.remote_base}/a.txt{self.stage_suffix}":
                self.local_md5s["a.txt"],
            f"{self.remote_base}/b.txt{self.stage_suffix}":
                self.local_md5s["b.txt"],
        })
        sftp.stat.side_effect = IOError("not found")

        def fake_rename(src, dst):
            if src.endswith(f"a.txt{self.stage_suffix}"):
                raise IOError("rename failed")
        sftp.rename.side_effect = fake_rename
        ok = sftp_sync.upload_atomic(
            sftp, sftp_client, self.local_base, self.remote_base,
            self.files, self.local_md5s, self.version)
        self.assertFalse(ok)

    def test_final_md5_mismatch_rolls_back_preship(self):
        sftp, sftp_client = self._mock_sftp_client({
            f"{self.remote_base}/a.txt{self.stage_suffix}":
                self.local_md5s["a.txt"],
            f"{self.remote_base}/b.txt{self.stage_suffix}":
                self.local_md5s["b.txt"],
            # Final mismatch for a.txt — cross-source detects
            f"{self.remote_base}/a.txt": "0" * 32,
            f"{self.remote_base}/b.txt": self.local_md5s["b.txt"],
        })
        # a.txt exists — will be snapshotted
        sftp.stat.side_effect = self._stat_side_effect(
            {f"{self.remote_base}/a.txt"})
        ok = sftp_sync.upload_atomic(
            sftp, sftp_client, self.local_base, self.remote_base,
            self.files, self.local_md5s, self.version)
        self.assertFalse(ok)
        # a.txt's preship should be restored: rename("a.txt.pre-ship-X", "a.txt")
        restore_calls = [
            c for c in sftp.rename.call_args_list
            if len(c.args) >= 2
            and c.args[1] == f"{self.remote_base}/a.txt"
        ]
        self.assertTrue(
            len(restore_calls) >= 1,
            f"preship restore should have been called, got: "
            f"{sftp.rename.call_args_list}")


class TestConnectSftpUsesRejectPolicy(unittest.TestCase):
    """BLOCKER 2 regression: RejectPolicy always, never WarningPolicy."""

    def _creds(self):
        return {"host": "x.com", "port": 22, "username": "u",
                "password": "p", "remotePath": "/r", "protocol": "sftp"}

    def test_known_hosts_loaded_with_reject_policy(self):
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as kh:
            kh.write("placeholder")
            kh_path = kh.name
        try:
            with patch("sftp_sync.paramiko.SSHClient") as MockClient:
                mock_client = MagicMock()
                MockClient.return_value = mock_client
                with patch("sftp_sync.find_known_hosts",
                           return_value=kh_path):
                    sftp_sync.connect_sftp(self._creds())
                policy = mock_client.set_missing_host_key_policy.call_args
                self.assertIsInstance(policy.args[0],
                                      paramiko.RejectPolicy)
                self.assertNotIsInstance(policy.args[0],
                                         paramiko.WarningPolicy)
        finally:
            os.unlink(kh_path)

    def test_no_known_hosts_uses_reject_policy(self):
        with patch("sftp_sync.paramiko.SSHClient") as MockClient:
            mock_client = MagicMock()
            MockClient.return_value = mock_client
            with patch("sftp_sync.find_known_hosts", return_value=None):
                sftp_sync.connect_sftp(self._creds())
            policy = mock_client.set_missing_host_key_policy.call_args
            self.assertIsInstance(policy.args[0],
                                  paramiko.RejectPolicy)
            self.assertNotIsInstance(policy.args[0],
                                     paramiko.WarningPolicy)


class TestCliRejectsInvalidVersion(unittest.TestCase):
    def test_invalid_version_exits(self):
        with patch("sys.argv", ["sftp_sync.py", "--version=bad",
                                "--dry-run"]):
            with self.assertRaises(SystemExit):
                sftp_sync.main()


class TestValidateRemotePathComponent(unittest.TestCase):
    """BLOCKER 7 regression: shell injection via remote path."""

    def test_valid_path(self):
        v = sftp_sync.validate_remote_path_component(
            "/remote/path/file-name_v1.0.exe", "name")
        self.assertEqual(v, "/remote/path/file-name_v1.0.exe")

    def test_empty_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_remote_path_component("", "name")

    def test_single_quote_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_remote_path_component(
                "/remote/'evil", "name")

    def test_semicolon_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_remote_path_component(
                "/remote/a;rm -rf /", "name")

    def test_backtick_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_remote_path_component(
                "/remote/`whoami`", "name")

    def test_dollar_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_remote_path_component(
                "/remote/$HOME", "name")

    def test_space_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_remote_path_component(
                "/remote/with space", "name")

    def test_newline_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_remote_path_component(
                "/remote/\n/foo", "name")

    def test_backslash_rejected(self):
        with self.assertRaises(ValueError):
            sftp_sync.validate_remote_path_component(
                "/remote/a\\b", "name")


class TestValidateConfigNoPlaceholders(unittest.TestCase):
    """BLOCKER 8 regression: refuse to run with unedited example config."""

    def test_clean_config_passes(self):
        cfg = {"sftpConfigPath": "/x/sftp.json",
               "localBase": "/x/releases",
               "remoteSubdir": "pinyin",
               "expectedMd5Source": "/x/_check.ps1"}
        # Should not raise / exit.
        sftp_sync.validate_config_no_placeholders(cfg)

    def test_REPLACE_WITH_rejected(self):
        cfg = {"sftpConfigPath": "REPLACE_WITH_PATH_TO_sftp.json",
               "localBase": "/x",
               "remoteSubdir": "pinyin",
               "expectedMd5Source": "/x/_check.ps1"}
        with self.assertRaises(SystemExit):
            sftp_sync.validate_config_no_placeholders(cfg)

    def test_path_to_bracket_rejected(self):
        cfg = {"sftpConfigPath": "/x/sftp.json",
               "localBase": "<path-to-local-staging>",
               "remoteSubdir": "pinyin",
               "expectedMd5Source": "/x/_check.ps1"}
        with self.assertRaises(SystemExit):
            sftp_sync.validate_config_no_placeholders(cfg)

    def test_YOUR_env_var_rejected(self):
        cfg = {"sftpConfigPath": "${YOUR_SFTP_JSON}",
               "localBase": "/x",
               "remoteSubdir": "pinyin",
               "expectedMd5Source": "/x/_check.ps1"}
        with self.assertRaises(SystemExit):
            sftp_sync.validate_config_no_placeholders(cfg)


class TestMakeRunId(unittest.TestCase):
    """BLOCKER 6 regression: unique suffix prevents collision with leftovers."""

    def test_format(self):
        rid = sftp_sync.make_run_id("0.19.0.59")
        self.assertTrue(rid.startswith("0.19.0.59-"))
        # suffix is timestamp (int)
        suffix = rid.split("-", 1)[1]
        self.assertTrue(suffix.isdigit())

    def test_unique_per_call(self):
        # Run with a tiny delay to ensure different timestamps.
        rid1 = sftp_sync.make_run_id("0.19.0.59")
        import time as _t
        _t.sleep(1.05)
        rid2 = sftp_sync.make_run_id("0.19.0.59")
        self.assertNotEqual(rid1, rid2)


class TestServerMd5UsesShlexQuote(unittest.TestCase):
    """BLOCKER 7 regression: server_md5 must shlex.quote() remote path."""

    def test_valid_path_quoted(self):
        client = MagicMock()
        stdout = MagicMock()
        stdout.readline.return_value = "ABCD" * 8 + "  /remote/a.txt\n"
        client.exec_command.return_value = (MagicMock(), stdout, MagicMock())
        result = sftp_sync.server_md5(client, "/remote/a.txt")
        self.assertEqual(result, "ABCD" * 8)
        # Path with no metacharacters doesn't need shlex quotes — but the cmd
        # MUST contain the path unmodified. Verify by using a path with a
        # shell metacharacter that requires quoting.
        cmd = client.exec_command.call_args.args[0]
        self.assertIn("/remote/a.txt", cmd)

    def test_shlex_quote_is_invoked(self):
        # Verify shlex.quote() is called for any valid remote path. The
        # validation layer already rejects shell metacharacters, so shlex.quote
        # here is defense-in-depth — verify it's actually invoked.
        client = MagicMock()
        stdout = MagicMock()
        stdout.readline.return_value = "ABCD" * 8 + "  /remote/a.txt\n"
        client.exec_command.return_value = (MagicMock(), stdout, MagicMock())
        with patch("sftp_sync.shlex.quote",
                   side_effect=lambda s: f"<QUOTED:{s}>") as mock_quote:
            sftp_sync.server_md5(client, "/remote/a.txt")
            mock_quote.assert_called_once_with("/remote/a.txt")
        cmd = client.exec_command.call_args.args[0]
        self.assertIn("<QUOTED:/remote/a.txt>", cmd)

    def test_unsafe_path_rejected(self):
        client = MagicMock()
        with self.assertRaises(ValueError):
            sftp_sync.server_md5(client, "/remote/'; rm -rf /; '.txt")
        # Verify exec_command was NOT called (no shell injection vector)
        client.exec_command.assert_not_called()


class TestSwapRestore(unittest.TestCase):
    """BLOCKER 5 regression: explicit swap (live → tmp → preship → live)."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.local_base = self.tmpdir
        self.remote_base = "/remote"
        self.version = "0.19.0.59"
        self.run_id = sftp_sync.make_run_id(self.version)
        self.preship_suffix = f"{sftp_sync.PRESHIP_SUFFIX}-{self.run_id}"
        self.filenames = ["a.txt"]
        for name in self.filenames:
            with open(os.path.join(self.local_base, name), "wb") as fh:
                fh.write(b"data-" + name.encode())
        self.local_md5s = {
            name: sftp_sync.md5_file(
                os.path.join(self.local_base, name))
            for name in self.filenames
        }
        self.files = [
            {"name": "a.txt", "expected": self.local_md5s["a.txt"]},
        ]

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def _mock_sftp_with_mismatch(self):
        """Mock sftp + client where final md5 of a.txt mismatches → rollback."""
        sftp = MagicMock()
        sftp_client = MagicMock()

        def exec(cmd):
            cmd_str = cmd if isinstance(cmd, str) else cmd.decode()
            stdout = MagicMock()
            if f"a.txt.stage-{self.run_id}" in cmd_str:
                stdout.readline.return_value = (
                    f"{self.local_md5s['a.txt']}  "
                    f"/remote/a.txt.stage-{self.run_id}\n"
                )
            elif "a.txt" in cmd_str:
                # Final: mismatch
                stdout.readline.return_value = (
                    "0" * 32 + "  /remote/a.txt\n"
                )
            else:
                stdout.readline.return_value = ""
            return MagicMock(), stdout, MagicMock()
        sftp_client.exec_command = exec
        # a.txt exists (so snapshot step creates pre-ship)
        sftp.stat.side_effect = lambda p: MagicMock() \
            if p == "/remote/a.txt" \
            else (_ for _ in ()).throw(IOError("not found"))
        return sftp, sftp_client

    def test_swap_restore_sequence(self):
        """Verify: live → tmp, preship → live, tmp removed (in that order)."""
        sftp, sftp_client = self._mock_sftp_with_mismatch()
        ok = sftp_sync.upload_atomic(
            sftp, sftp_client, self.local_base, self.remote_base,
            self.files, self.local_md5s, self.version)
        self.assertFalse(ok)
        rename_calls = sftp.rename.call_args_list
        # Step A: live → rollback_tmp (must occur before restore)
        rollback_renames = [
            c for c in rename_calls
            if c.args[0] == "/remote/a.txt"
            and "rollback" in c.args[1]
        ]
        self.assertTrue(
            len(rollback_renames) >= 1,
            f"Expected live → rollback_tmp rename, got: {rename_calls}")
        # Step B: preship → live (must occur after A)
        restore_renames = [
            c for c in rename_calls
            if c.args[0] == f"/remote/a.txt{self.preship_suffix}"
            and c.args[1] == "/remote/a.txt"
        ]
        self.assertTrue(
            len(restore_renames) >= 1,
            f"Expected preship → live rename, got: {rename_calls}")
        # Step C: rollback_tmp was removed
        removed = [c.args[0] for c in sftp.remove.call_args_list]
        self.assertTrue(
            any("rollback" in p for p in removed),
            f"Expected rollback_tmp removal, got: {removed}")
        # Order check: live → rollback_tmp must precede preship → live
        idx_a = next(i for i, c in enumerate(rename_calls)
                     if c.args[0] == "/remote/a.txt"
                     and "rollback" in c.args[1])
        idx_b = next(i for i, c in enumerate(rename_calls)
                     if c.args[0] == f"/remote/a.txt{self.preship_suffix}"
                     and c.args[1] == "/remote/a.txt")
        self.assertLess(idx_a, idx_b,
                        f"swap_restore out of order: A@{idx_a} B@{idx_b}")


class TestUniqueRunIdNoCollision(unittest.TestCase):
    """BLOCKER 6 regression: per-run suffix prevents preship collision."""

    def test_two_ships_have_different_suffix(self):
        rid1 = sftp_sync.make_run_id("0.19.0.59")
        import time as _t
        _t.sleep(1.05)
        rid2 = sftp_sync.make_run_id("0.19.0.59")
        self.assertNotEqual(rid1, rid2)
        # Suffixes built from run_ids don't collide
        suf1 = f"{sftp_sync.PRESHIP_SUFFIX}-{rid1}"
        suf2 = f"{sftp_sync.PRESHIP_SUFFIX}-{rid2}"
        self.assertNotEqual(suf1, suf2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
