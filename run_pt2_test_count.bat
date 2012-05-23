::::::::::::::::::::::::::::::
:: Batch file configuration ::
::::::::::::::::::::::::::::::
@ECHO OFF
:: Set where program be
SET NAME_APP=pt2_test_count_win.exe

:: Set filepath of input PT2
SET NAME_IN=g://pt2/e001.pt2

:: Set filepath of output
SET NAME_OUT=out.out

CALL "%NAME_APP%" "%NAME_IN%" "%NAME_OUT%"

ECHO.
ECHO. ---------------------------------------
ECHO. APP=%NAME_APP%
ECHO.
ECHO. INPUT=%NAME_IN%
ECHO.
ECHO. OUTPUT=%NAME_OUT%
ECHO.
ECHO. ---------------------------------------
ECHO. press any key to continue...

PAUSE>NUL
ECHO ON