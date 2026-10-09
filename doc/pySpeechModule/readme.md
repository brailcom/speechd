# building the docs locally.

```
python -m venv ./venv
source venv/bin/activate

pip install -U sphinx
pip install sphinx-autobuild
pip install sphinx-rtd-theme myst-parser

# using sphinx
sphinx-build -M html source build

# using autobuild
sphinx-autobuild --host 0.0.0.0 --port 8080 source build
# go to localhost:8080/ to see a live preview of the doc's
```