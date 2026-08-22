# Host-side native test suite

Runs in this repository (`sempermechanics/semper-dic-engine`). The private Android
app consumes a pinned submodule and does **not** re-run these host tiers.

```bash
# From the engine repo root
git submodule update --init --recursive
cmake -S tests -B build/tests -DCMAKE_BUILD_TYPE=Release -DDIC_REQUIRE_OPENCV=ON
cmake --build build/tests
./build/tests/dic_tests
```

CI: `.github/workflows/ci.yml` (host tests, sanitizers, C SDK smoke).

See [docs/TESTING.md](../docs/TESTING.md) for the catalog.
