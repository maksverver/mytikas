This directory contains the code for the browser frontend.


BUILDING
--------

Building requires compiling the C++ library to Web Assembly using Emscripten.
To do this run, the build-wasm-api.sh script in the root of the project:

```
% ../build-wasm-api.sh
``

This requires Emscripten to be installed. It builds two files: wasm-api.js and
wasm-api.wasm, and copies them into the www/ subdirectory.

Note that the script needs to be rerun whenever the C++ code changes.


RUNNING
-------

To run the web application, serve the files from this directory, e.g. with:

```
% python3 -m http.server -b localhost 8080
````

Now the web app should be available at http://localhost:8080/
