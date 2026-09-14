param([string]$Compiler = "gcc")
$ErrorActionPreference = "Stop"
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../.."))
$taskOutput = Join-Path $taskRoot "tmp/can-tests"
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
$taskIncludes = @("tests/can/stubs", "tests/can", "GS_flowmeter/Can") | ForEach-Object { "-I" + (Join-Path $taskRoot $_) }
$taskSources = @("tests/can/A_CanUserTest.c", "tests/can/H_CanMock.c",
    "GS_flowmeter/Can/F_CanUser.c", "GS_flowmeter/Can/F_HostCan.c",
    "GS_flowmeter/Can/H_HostCan.c", "GS_flowmeter/Can/A_HostCan.c") |
    ForEach-Object { Join-Path $taskRoot $_ }
$taskExecutable = Join-Path $taskOutput "can_user_test.exe"
& $Compiler -std=c99 -O2 -g -Wall -Wextra -Werror -Wconversion -Wshadow @taskIncludes @taskSources -o $taskExecutable
if ($LASTEXITCODE -ne 0) { throw "CAN_USER test compilation failed." }
& $taskExecutable
if ($LASTEXITCODE -ne 0) { throw "CAN_USER tests failed." }

