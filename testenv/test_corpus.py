import json
import pytest
from corpus import PROFILE, issues, key, measure, verify

EXPECTED = json.loads(PROFILE.read_text())


@pytest.fixture(scope="module")
def corpus_rows():
    rows = measure()
    verify(rows)
    return {key(row): row for row in rows}


@pytest.mark.parametrize("name", [k for k, p in EXPECTED.items() if not p["issues"]])
def test_proven_upstream_roundtrip(corpus_rows, name):
    assert not issues(corpus_rows[name])


def test_documented_corpus_limitations(corpus_rows):
    # This characterizes known failures; it does not claim their acceptance.
    for name, profile in EXPECTED.items():
        if profile["issues"]:
            assert issues(corpus_rows[name]) == profile["issues"]
