@echo off

REM http://localhost:8000/calculi.html

cd build_emscripten\Release\bin
python -m http.server
cd ..\..\..
