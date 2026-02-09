@echo off
setlocal

if "%~1"=="" (
    echo Usage: tag-push.bat ^<tag-name^>
    echo Example: tag-push.bat v0.7.0
    exit /b 1
)

set TAG_NAME=%~1

echo Removing tag %TAG_NAME% from remote if it exists...
git push origin :refs/tags/%TAG_NAME% 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Remote tag %TAG_NAME% removed successfully.
) else (
    echo Remote tag %TAG_NAME% does not exist or could not be removed.
)

echo.
echo Removing local tag %TAG_NAME% if it exists...
git tag -d %TAG_NAME% 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Local tag %TAG_NAME% removed successfully.
) else (
    echo Local tag %TAG_NAME% does not exist.
)

echo.
echo Creating new tag %TAG_NAME%...
git tag %TAG_NAME%
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to create tag %TAG_NAME%
    exit /b 1
)

echo.
echo Pushing tag %TAG_NAME% to remote...
git push origin %TAG_NAME%
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to push tag %TAG_NAME%
    exit /b 1
)

echo.
echo Successfully created and pushed tag %TAG_NAME%!
exit /b 0
