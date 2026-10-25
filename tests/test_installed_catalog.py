import hashlib
import importlib.util
from pathlib import Path
import sys
import unittest

TOOLS=Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0,str(TOOLS))
import verify_installed_catalog as mapping


class InstalledCatalogTests(unittest.TestCase):
    def test_exact_chart_bytes_bind_each_difficulty(self):
        raw=b'{"synthetic":true}'
        digest=hashlib.sha256(raw).hexdigest()
        for difficulty in ('IN','AT'):
            row=dict(difficulty=difficulty,chart=dict(sha256=digest))
            self.assertEqual(mapping.bind_chart(row,raw),digest)

    def test_different_revision_is_never_aliased(self):
        row=dict(difficulty='AT',chart=dict(sha256='a'*64))
        with self.assertRaisesRegex(ValueError,'bytes differ'):
            mapping.bind_chart(row,b'other source')

    def test_difficulty_is_not_inferred_from_title(self):
        row=dict(difficulty='IN/AT',chart=dict(sha256='a'*64))
        with self.assertRaisesRegex(ValueError,'IN/AT'):
            mapping.bind_chart(row,b'raw')

if __name__=='__main__':unittest.main()
