"""Offline contracts for the catalogue WinForms front end.

Set PAS_TEST_PWSH to an explicit PowerShell 7.2+ executable to run these on
Linux or Windows. They never load WinForms, launch a native game tool, access
a device, or request network content.
"""
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PANEL = Path(__file__).resolve().parents[1] / "tools" / "song-library-panel.ps1"
PWSH = os.environ.get("PAS_TEST_PWSH") or shutil.which("pwsh")


@unittest.skipUnless(PWSH, "PowerShell 7.2+ is not installed; set PAS_TEST_PWSH")
class SongLibraryPanelTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pas-catalog-ui-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "曲庫 with spaces"
        (self.root / "charts").mkdir(parents=True)
        self.raw = b'{"formatVersion":3,"synthetic":true}'
        self.sha = hashlib.sha256(self.raw).hexdigest()
        (self.root / "charts" / "native.json").write_bytes(self.raw)
        self.library = self.root / "library.json"
        self.rows = [
            self.row("a-in", "alpha", "IN", "0", "光與星", "O'Brien"),
            self.row("a-at", "alpha", "AT", "0", "光與星", "O'Brien"),
            self.row("a-variant", "alpha", "IN", "1", "光與星", "O'Brien"),
            self.row("b-in", "beta", "IN", "0", "Literal [*]", "Another"),
        ]
        self.data = dict(
            schema="pas.catalog-library.v1",
            source=dict(repository="example/synthetic", revision="a" * 40),
            charts=self.rows,
            counts=dict(song_count=2, chart_count=4),
        )
        self.write_library()

    def row(self, identity, song, difficulty, variant, title, composer):
        return dict(
            library_id=identity,
            song_id=song,
            track_id=f"{song}.Artist.{variant}",
            title=title,
            composer=composer,
            difficulty=difficulty,
            variant=variant,
            version="4.0.1.2",
            chart=dict(
                file="charts/native.json",
                sha256=self.sha,
                bytes=len(self.raw),
                git_blob_sha="a" * 40,
                raw_url="https://raw.githubusercontent.com/example/synthetic/commit/chart.json",
            ),
            source=dict(
                repository="example/synthetic",
                revision="a" * 40,
                game_version="4.0.1.2",
                game_version_exact=False,
            ),
            archive_status="verified",
            analysis_status="structural_only",
            prepare_status="blocked",
            live_ready=False,
            notes=3,
            line_count=1,
            format_version=3,
            blockers=[
                dict(code="version_match_required", message="來源版本與本機尚未比對。"),
                dict(code="local_profile_required", message="尚未建立本機實測設定。"),
            ],
        )

    def write_library(self):
        self.library.write_text(
            json.dumps(self.data, ensure_ascii=False), encoding="utf-8"
        )

    def invoke(self, code, *, target="", expected=""):
        driver = self.root / "driver.ps1"
        driver.write_text(
            "param([string]$Panel, [string]$CatalogPath, [string]$Target, [string]$Expected)\n"
            "$ErrorActionPreference = 'Stop'\n"
            ". $Panel -LoadCoreOnly\n" + code,
            encoding="utf-8",
        )
        result = subprocess.run(
            [
                PWSH,
                "-NoProfile",
                "-NonInteractive",
                "-File",
                str(driver),
                "-Panel",
                str(PANEL),
                "-CatalogPath",
                str(self.library),
                "-Target",
                str(target),
                "-Expected",
                str(expected),
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=30,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr or result.stdout)
        return json.loads(result.stdout)

    def assert_read_rejected(self):
        value = self.invoke(
            "try { Read-SongCatalog $CatalogPath | Out-Null; $rejected=$false } "
            "catch { $rejected=$true }; @{rejected=$rejected}|ConvertTo-Json"
        )
        self.assertTrue(value["rejected"])

    def test_parser_and_headless_self_test(self):
        value = self.invoke(
            "$tokens=$null; $errors=$null; "
            "[Management.Automation.Language.Parser]::ParseFile($Panel,[ref]$tokens,[ref]$errors)|Out-Null; "
            "if($errors.Count){throw ($errors -join '; ')}; Invoke-CatalogCoreTest|ConvertTo-Json"
        )
        self.assertEqual(value["status"], "passed")
        self.assertGreaterEqual(value["checks"], 14)
        self.assertEqual(value["windows_shown"], 0)
        self.assertEqual(value["native_processes_started"], 0)
        self.assertFalse(value["device_access"])

    def test_relative_paths_follow_powershell_location_not_process_directory(self):
        elsewhere = Path(self.temp.name) / "different process directory"
        elsewhere.mkdir()
        value = self.invoke(
            "[Environment]::CurrentDirectory=$Target; "
            "Set-Location -LiteralPath (Split-Path -Parent $CatalogPath); "
            "$m=Read-SongCatalog 'library.json'; "
            "$raw=Read-CatalogJson 'library.json'; "
            "$verified=Test-CatalogAsset 'charts/native.json' $Expected; "
            "@{path=$m.Path; charts=$m.ChartCount; schema=$raw.schema; verified=$verified.Verified; "
            "preview=(Resolve-CatalogInputPath 'outputs/new-preview'); "
            "process_cwd=[Environment]::CurrentDirectory}|ConvertTo-Json",
            target=elsewhere,
            expected=self.sha,
        )
        # Windows temp paths may use RUNNER~1 while PowerShell returns the long
        # spelling. Compare actual identity without weakening the cwd contract.
        self.assertTrue(os.path.samefile(value["path"], self.library))
        self.assertEqual(value["charts"], 4)
        self.assertEqual(value["schema"], "pas.catalog-library.v1")
        self.assertTrue(value["verified"])
        preview = Path(value["preview"])
        self.assertEqual(preview.parts[-2:], ("outputs", "new-preview"))
        self.assertTrue(os.path.samefile(preview.parent.parent, self.root))
        self.assertFalse(preview.exists())
        self.assertTrue(os.path.samefile(value["process_cwd"], elsewhere))

    def test_multi_difficulty_variant_counts_and_literal_unicode_search(self):
        value = self.invoke(
            "$m=Read-SongCatalog $CatalogPath; "
            "@{songs=$m.SongCount; charts=$m.ChartCount; IN=$m.IN; AT=$m.AT; "
            "unicode=@(Select-CatalogRows $m.Rows \"光 o'brien\" 'AT').Id; "
            "variant=@(Select-CatalogRows $m.Rows '' 'IN' '1').Id; "
            "literal=@(Select-CatalogRows $m.Rows '[*]').Id; "
            "empty=@(Select-CatalogRows $m.Rows 'not present').Count}|ConvertTo-Json"
        )
        self.assertEqual((value["songs"], value["charts"], value["IN"], value["AT"]), (2, 4, 3, 1))
        self.assertEqual(value["unicode"], "a-at")
        self.assertEqual(value["variant"], "a-variant")
        self.assertEqual(value["literal"], "b-in")
        self.assertEqual(value["empty"], 0)

    def test_duplicate_identity_and_false_live_qualification_rejected(self):
        original = copy.deepcopy(self.data)
        self.data["charts"][1]["library_id"] = self.data["charts"][0]["library_id"]
        self.write_library()
        self.assert_read_rejected()
        self.data = copy.deepcopy(original)
        self.data["charts"][0]["live_ready"] = True
        self.write_library()
        self.assert_read_rejected()
        self.data = copy.deepcopy(original)
        self.data["charts"][0]["live_ready"] = "false"
        self.write_library()
        self.assert_read_rejected()

    def test_wrong_counts_and_non_chart_paths_rejected(self):
        original = copy.deepcopy(self.data)
        self.data["counts"]["chart_count"] = 5
        self.write_library()
        self.assert_read_rejected()
        for bad in ("../outside.json", "charts/../../outside.json", "/outside.json", "charts/program.exe"):
            with self.subTest(path=bad):
                self.data = copy.deepcopy(original)
                self.data["charts"][0]["chart"]["file"] = bad
                self.write_library()
                self.assert_read_rejected()

    def test_version_and_blockers_stay_visible_without_claiming_playability(self):
        value = self.invoke(
            "$m=Read-SongCatalog $CatalogPath; "
            "@{text=(Format-CatalogDetails $m.Rows[0]); states=@($m.Rows.StateCode); "
            "pending=@(Select-CatalogRows $m.Rows '' '' '' 'local_pending').Count}|ConvertTo-Json"
        )
        self.assertIn("4.0.1.2", value["text"])
        self.assertIn("來源版本與本機尚未比對", value["text"])
        self.assertIn("尚未建立本機實測設定", value["text"])
        self.assertEqual(value["states"], ["local_pending"] * 4)
        self.assertEqual(value["pending"], 4)

    def test_unsupported_native_format_remains_browsable_and_filterable(self):
        self.data["charts"][0]["analysis_status"] = "unsupported_format"
        self.data["charts"][0]["format_version"] = 1
        self.data["charts"][0]["blockers"].insert(
            0, dict(code="unsupported_format", message="原件已封存；原生格式 v1 尚待支援。")
        )
        self.write_library()
        value = self.invoke(
            "$m=Read-SongCatalog $CatalogPath; "
            "@{all=$m.ChartCount; unsupported=@(Select-CatalogRows $m.Rows '' '' '' 'unsupported').Id; "
            "text=(Format-CatalogDetails $m.Rows[0])}|ConvertTo-Json"
        )
        self.assertEqual(value["all"], 4)
        self.assertEqual(value["unsupported"], "a-in")
        self.assertIn("原生格式 v1 尚待支援", value["text"])

    def test_materialization_pointer_and_missing_states_override_archive_label(self):
        asset = self.root / "charts" / "native.json"
        asset.write_text("version https://git-lfs.github.com/spec/v1\n", encoding="utf-8")
        value = self.invoke(
            "$m=Read-SongCatalog $CatalogPath; "
            "@{states=@($m.Rows.StateCode); count=@(Select-CatalogRows $m.Rows '' '' '' 'lfs_pointer').Count}|ConvertTo-Json"
        )
        self.assertEqual(value["states"], ["lfs_pointer"] * 4)
        self.assertEqual(value["count"], 4)
        asset.unlink()
        value = self.invoke(
            "$m=Read-SongCatalog $CatalogPath; @{states=@($m.Rows.StateCode)}|ConvertTo-Json"
        )
        self.assertEqual(value["states"], ["missing"] * 4)

    def test_background_loader_and_hash_verifier_return_single_results(self):
        value = self.invoke(
            "$job=New-CatalogReadOperation $Panel 'load' @($CatalogPath); "
            "try { $r=$job.Shell.EndInvoke($job.Async); if($job.Shell.HadErrors){throw $job.Shell.Streams.Error[0]}; "
            "$count=$r.Count; $charts=$r[0].ChartCount } finally {$job.Shell.Dispose()}; "
            "$job=New-CatalogReadOperation $Panel 'verify' @($Target,$Expected); "
            "try { $r=$job.Shell.EndInvoke($job.Async); if($job.Shell.HadErrors){throw $job.Shell.Streams.Error[0]}; "
            "@{results=$count; charts=$charts; verified=$r[0].Verified; hash=$r[0].Sha256}|ConvertTo-Json } "
            "finally {$job.Shell.Dispose()}",
            target=self.root / "charts" / "native.json",
            expected=self.sha,
        )
        self.assertEqual(value["results"], 1)
        self.assertEqual(value["charts"], 4)
        self.assertTrue(value["verified"])
        self.assertEqual(value["hash"], self.sha)

    def test_native_import_audit_is_bound_and_never_grants_gameplay(self):
        report_dir = self.root / "manifests"
        report_dir.mkdir()
        report = dict(
            schema="pas.catalog-native-import-report.v1",
            gameplay_qualified=False,
            library=dict(
                file="library.json",
                sha256=hashlib.sha256(self.library.read_bytes()).hexdigest(),
            ),
            charts=[
                dict(
                    library_id=row["library_id"],
                    chart_file=row["chart"]["file"],
                    chart_sha256=row["chart"]["sha256"],
                    gameplay_qualified=False,
                    status="rejected" if i == 1 else "passed",
                    stderr="synthetic reversed event interval" if i == 1 else "",
                )
                for i, row in enumerate(self.rows)
            ],
        )
        (report_dir / "native-import-report.json").write_text(
            json.dumps(report), encoding="utf-8"
        )
        result = self.invoke(
            "$m=Read-SongCatalog $CatalogPath; "
            "@{states=@($m.Rows.StateCode); native=@($m.Rows.NativeImportStatus); "
            "rejected=@(Select-CatalogRows $m.Rows '' '' '' 'import_rejected').Id; "
            "text=(Format-CatalogDetails $m.Rows[1])}|ConvertTo-Json"
        )
        self.assertEqual(result["native"], ["passed", "rejected", "passed", "passed"])
        self.assertEqual(result["rejected"], "a-at")
        self.assertEqual(result["states"], ["local_pending", "import_rejected", "local_pending", "local_pending"])
        self.assertIn("起訖倒序", result["text"])
        self.assertIn("synthetic reversed event interval", result["text"])
        self.data["charts"][0]["composer"] = "Changed metadata"
        self.write_library()
        result = self.invoke(
            "$m=Read-SongCatalog $CatalogPath; @{native=@($m.Rows.NativeImportStatus); "
            "status=$m.NativeAuditStatus}|ConvertTo-Json"
        )
        self.assertEqual(result["native"], ["not_audited"] * 4)
        self.assertIn("未套用", result["status"])

    def test_legacy_entry_validates_script_hash_and_manual_flags_only(self):
        gui = self.root / "sweep-panel.ps1"
        gui.write_text("# synthetic, never executed\n", encoding="utf-8")
        config = self.root / "launch-config.json"
        value = dict(
            schema="pas.manual-song-panel-launch.v1",
            manual_selection=True,
            manual_start=True,
            gui_script=dict(file=str(gui), sha256=hashlib.sha256(gui.read_bytes()).hexdigest()),
        )
        config.write_text(json.dumps(value), encoding="utf-8")
        result = self.invoke(
            "Get-CatalogLegacyLaunch $Target | ConvertTo-Json", target=config
        )
        self.assertEqual(Path(result["Script"]), gui)
        gui.write_text("# changed\n", encoding="utf-8")
        result = self.invoke(
            "try { Get-CatalogLegacyLaunch $Target|Out-Null; $rejected=$false } "
            "catch {$rejected=$true}; @{rejected=$rejected}|ConvertTo-Json",
            target=config,
        )
        self.assertTrue(result["rejected"])
        value["manual_start"] = False
        config.write_text(json.dumps(value), encoding="utf-8")
        result = self.invoke(
            "try { Get-CatalogLegacyLaunch $Target|Out-Null; $rejected=$false } "
            "catch {$rejected=$true}; @{rejected=$rejected}|ConvertTo-Json",
            target=config,
        )
        self.assertTrue(result["rejected"])


if __name__ == "__main__":
    unittest.main()
