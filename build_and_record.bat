@echo off
REM filepath: c:\Users\prath\OneDrive\Documents\College\Year_2\SEM_4\OS\edurtos\build_and_record.bat
echo Building EduRTOS with output recording...

REM Create build directory
if not exist build mkdir build

REM Go to build directory
cd build

REM Configure with CMake
echo Configuring with CMake...
cmake .. -G "MinGW Makefiles"

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed with error level %ERRORLEVEL%
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

REM Build
echo Building project...
cmake --build .

if %ERRORLEVEL% NEQ 0 (
    echo Build failed with error level %ERRORLEVEL%
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Build successful! 

echo.
echo === Running example with console output recording ===
echo Program will run for 25 seconds - please wait...

REM Use timeout command with /nobreak to run the application for exactly 25 seconds
start /b "" .\edurtos_example.exe
timeout /t 25 /nobreak
taskkill /F /IM edurtos_example.exe >nul 2>&1

echo.
echo Main example completed! Output saved to edurtos_output.txt

echo.
echo === Running test tasks with console output recording ===
echo Program will run for 25 seconds - please wait...

REM Use timeout command with /nobreak to run the application for exactly 25 seconds
start /b "" .\edurtos_tests.exe
timeout /t 25 /nobreak
taskkill /F /IM edurtos_tests.exe >nul 2>&1

echo.
echo Test tasks completed! Output saved to test_tasks_output.txt

REM Return to main directory
cd ..

echo.
echo Test run completed.
echo You can find the output files in the build directory:
echo - build\edurtos_output.txt
echo - build\test_tasks_output.txt
echo - build\scheduler_log.csv
echo - build\scheduler_decisions.csv

REM Copy output files to main directory
echo.
echo Copying output files to project root directory...
copy build\edurtos_output.txt .
copy build\test_tasks_output.txt .
copy build\scheduler_log.csv .
copy build\scheduler_decisions.csv .

echo.
echo Files copied successfully.
echo.
echo Testing complete! Press any key to exit.
pause