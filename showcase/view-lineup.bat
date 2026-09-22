@echo off
REM Serve the worktree and open the lineup. Double-click this.
REM A server is needed because the pages reference ../shots/ and each other;
REM opening the HTML straight off disk leaves Chrome's file:// rules to decide
REM whether the previews render, and they often don't.

cd /d "%~dp0\.."

REM Start a server. If one is already listening on 4181 this just exits quietly.
start "s2x-showcase-server" /min python -m http.server 4181 --bind 127.0.0.1

REM Give it a moment to bind before the browser asks for a page.
timeout /t 2 /nobreak >nul

start "" "http://127.0.0.1:4181/showcase/lineup.html"
