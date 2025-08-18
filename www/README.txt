This directory contains the code for the browser frontend, using React +
TypeScript + Vite for the client code, and Emscripten to compile the C++ game
logic to Web Assembly.


BUILDING
--------

Building requires compiling the C++ library to Web Assembly using Emscripten.
To do this run, the build-wasm-api.sh script in the root of the project:

```
% ../build-wasm-api.sh
``

This requires Emscripten to be installed. It builds two files: wasm-api.js and
wasm-api.wasm, and copies them into the www/generated/ subdirectory.

Note that the script needs to be rerun whenever the C++ code changes.

To build the web app, run:

```
% npm run build
```

This writes the generated files to the dist/ subdirectory.


DEVELOPMENT
-----------

To run the web application locally:

```
% npm run dev
````

Now the web app should be available at http://localhost:5173/
