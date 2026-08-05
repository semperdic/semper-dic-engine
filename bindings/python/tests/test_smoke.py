"""Smoke test: a known rigid translation must be recovered.

Mirrors the C++ golden `test_translation_synthetic` at the Python boundary — it
proves the bindings marshal images/params/results correctly, not the DIC math
itself (that is covered by the engine's own suite).
"""
import numpy as np
import pytest

import semper


def _speckle(h=256, w=256, seed=0):
    rng = np.random.default_rng(seed)
    # Blobby speckle: smooth noise thresholded, so ICGN has gradients to lock onto.
    base = rng.random((h, w)).astype(np.float32)
    k = np.ones((3, 3), np.float32) / 9.0
    for _ in range(3):  # cheap box blur
        base = np.pad(base, 1, mode="edge")
        base = (
            base[:-2, :-2] + base[:-2, 1:-1] + base[:-2, 2:]
            + base[1:-1, :-2] + base[1:-1, 1:-1] + base[1:-1, 2:]
            + base[2:, :-2] + base[2:, 1:-1] + base[2:, 2:]
        ) / 9.0
    return (base * 255).astype(np.uint8)


def test_version_exposed():
    assert isinstance(semper.__version__, str)
    assert semper.__version__.count(".") == 2


def test_recovers_rigid_translation():
    ref = _speckle()
    dx = 3
    deformed = np.roll(ref, shift=dx, axis=1)  # shift right by dx px

    eng = semper.Engine()
    eng.set_reference(ref)
    res = eng.run(deformed, rect=(40, 40, 176, 176), step=20, subset=31, strain_window=5)

    assert res.count > 0
    assert res.points.shape[1] == 8
    assert res.metrics.shape == (17,)
    # Column 2 is u (x-displacement). Recovered median should match the shift.
    u = res.points[:, 2]
    assert np.median(u) == pytest.approx(dx, abs=0.5)


def test_missing_reference_errors():
    eng = semper.Engine()
    with pytest.raises(semper.DicError):
        eng.run(_speckle(), rect=(0, 0, 128, 128), step=20, subset=31, strain_window=5)
