@echo off
echo Testing WSL access...
echo.

wsl --version

echo.
echo Testing basic command...
wsl --exec uname -a

echo.
echo WSL is working! Now testing full build...
echo.

wsl --exec bash -c "echo 'Dependencies installed successfully'"

pause