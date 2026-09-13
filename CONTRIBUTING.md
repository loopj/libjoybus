# Contributing

First off, thanks for taking the time to contribute!

## Reporting issues

Please include the backend and board you are using, the device on the other end of the bus, and the version or commit of libjoybus. For a timing or communication problem, a logic analyzer capture of the exchange is the fastest way to get it diagnosed.

## Submit your work

Submit your improvements, fixes, and new features one at a time, using GitHub [Pull Requests](https://docs.github.com/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests).

Good pull requests remain focused in scope and avoid containing unrelated commits. If your contribution involves a significant amount of work or substantial changes to any part of the project, please open an issue to discuss it first to avoid any wasted or duplicate effort.

## Implementing a Backend

A backend adapts libjoybus to a specific microcontroller family. See [`src/backend/README.md`](src/backend/README.md) for the structure of a backend, the behavior and timing it must implement, and the OEM device measurements those requirements are derived from.

## Running tests

Most of the higher level functionality of `libjoybus` can be tested without
running on an embedded backend.

Build the test suite

```bash
cmake -Bbuild && cmake --build build
```

Run the tests

```bash
ctest --test-dir build --output-on-failure
```
