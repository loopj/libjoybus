html:
	.venv/bin/sphinx-build -b html . _build/html

livehtml:
	.venv/bin/sphinx-autobuild --ignore "*/.venv/*" . _build/html

clean:
	rm -rf _build

.PHONY: html livehtml clean
