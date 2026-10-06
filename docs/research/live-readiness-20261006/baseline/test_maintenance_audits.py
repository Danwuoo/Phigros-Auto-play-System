#!/usr/bin/env python3
"""Maintenance audit tests; mutations occur only in a disposable evidence copy."""
import importlib.util,json,pathlib,shutil,subprocess,tempfile,unittest
HERE=pathlib.Path(__file__).resolve().parent
REPO=HERE.parents[3]
spec=importlib.util.spec_from_file_location('compact',HERE/'audit_compact_zip.py');compact=importlib.util.module_from_spec(spec);spec.loader.exec_module(compact)

class AuditTests(unittest.TestCase):
    def test_safe_relative_path(self):
        self.assertEqual(compact.safe_name('one/two.txt').as_posix(),'one/two.txt')
    def test_traversal_absolute_noncanonical_and_windows_unsafe(self):
        for path in ['../x','one/../../x','/root/x','C:/x','one\\two','one//two','one/./two','one/','bad\x00x','x./a','x /a','a/evil:stream','a/b?x','a/b\nx']:
            with self.subTest(path=path),self.assertRaises(AssertionError):compact.safe_name(path)
    def test_aux_is_reported_not_erased_from_posix_quarantine(self):
        self.assertEqual(compact.safe_name('snapshot/aux/CMakeLists.txt').parts[1],'aux')
        self.assertTrue(compact.RESERVED.match('aux'))
        self.assertTrue(compact.RESERVED.match('COM1.txt'))
        self.assertFalse(compact.RESERVED.match('auxiliary'))
    def test_modified_receipt_rejected_without_editing_original(self):
        original=REPO/'docs/research/zero-miss-20261006/windows'
        protected=original/'prelive-evidence-01/package/current_tests-release-14.json'
        before=compact.sha_file(protected)
        with tempfile.TemporaryDirectory(prefix='phigros-audit-negative-') as td:
            root=pathlib.Path(td);target=root/'docs/research/zero-miss-20261006/windows'
            shutil.copytree(original,target)
            (root/'.git').write_text('gitdir: '+str(REPO/'.git')+'\n')
            changed=target/'prelive-evidence-01/package/current_tests-release-14.json'
            d=json.loads(changed.read_text());d['checks'][0]['pass']=False;changed.write_text(json.dumps(d)+'\n')
            report=root/'report.json'
            result=subprocess.run(['python',str(HERE/'audit_baseline.py'),'--repo',str(root),'--output',str(report)],capture_output=True,text=True)
            self.assertEqual(result.returncode,1,result.stderr)
            data=json.loads(report.read_text());self.assertFalse(data['verification_pass'])
            self.assertIn('seal:prelive-evidence-01:SHA256_INDEX_FINAL.json',data['failed_checks'])
            self.assertIn('accepted_regression:release:current_tests',data['failed_checks'])
        self.assertEqual(compact.sha_file(protected),before)

if __name__=='__main__':unittest.main(verbosity=2)
