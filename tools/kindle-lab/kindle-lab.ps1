[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet(
        'Connection', 'Deploy', 'Inventory', 'InstallHooks', 'Run', 'Status',
        'Stop', 'KillStockfish', 'Framebuffer', 'Collect', 'ProcessCheck',
        'InputStart', 'Tap', 'InputDown', 'InputMove', 'InputUp', 'InputStop'
    )]
    [string]$Action,

    [string]$RunId,
    [string]$Package,
    [ValidateSet('TERM', 'KILL')]
    [string]$Signal = 'TERM',
    [ValidateRange(0, 1071)]
    [int]$X = 0,
    [ValidateRange(0, 1447)]
    [int]$Y = 0,
    [ValidateRange(1, 10000)]
    [int]$DurationMs = 30,
    [string]$ConfigPath = (Join-Path $PSScriptRoot 'config.local.psd1')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$settings = @{
    Host = '192.168.1.4'
    Port = 2022
    User = 'root'
    EvidenceRoot = (Join-Path $env:LOCALAPPDATA 'kindlebrew-lab\evidence')
}
if (Test-Path -LiteralPath $ConfigPath) {
    $configured = Import-PowerShellDataFile -LiteralPath $ConfigPath
    foreach ($key in @('Host', 'Port', 'User', 'EvidenceRoot')) {
        if ($configured.ContainsKey($key)) {
            $settings[$key] = $configured[$key]
        }
    }
}

$labHost = [string]$settings.Host
$labUser = [string]$settings.User
$labPort = [int]$settings.Port
$evidenceRoot = [string]$settings.EvidenceRoot
if ($labHost -notmatch '^[A-Za-z0-9.-]+$') { throw 'Unsafe SSH host in lab configuration.' }
if ($labUser -notmatch '^[A-Za-z0-9._-]+$') { throw 'Unsafe SSH user in lab configuration.' }
if ($labPort -lt 1 -or $labPort -gt 65535) { throw 'Invalid SSH port in lab configuration.' }

$buildDir = Join-Path $PSScriptRoot 'build'
$lastRunPath = Join-Path $buildDir 'last-run.txt'
if ([string]::IsNullOrWhiteSpace($RunId)) {
    if ($Action -eq 'Connection') {
        $RunId = 'connection-check'
    } elseif ($Action -eq 'Deploy') {
        $RunId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-inkchess'
    } elseif (Test-Path -LiteralPath $lastRunPath) {
        $RunId = (Get-Content -LiteralPath $lastRunPath -Raw).Trim()
    } else {
        throw 'Pass -RunId, or run Deploy first so the harness can remember one.'
    }
}
if ($RunId -notmatch '^[A-Za-z0-9._-]+$') { throw 'Unsafe run id.' }

$windowsOpenSsh = Join-Path $env:WINDIR 'System32\OpenSSH'
$sshPath = Join-Path $windowsOpenSsh 'ssh.exe'
$scpPath = Join-Path $windowsOpenSsh 'scp.exe'
if (-not (Test-Path -LiteralPath $sshPath)) {
    $sshPath = (Get-Command ssh.exe -ErrorAction Stop).Source
}
if (-not (Test-Path -LiteralPath $scpPath)) {
    $scpPath = (Get-Command scp.exe -ErrorAction Stop).Source
}
if ($sshPath -match '(?i)wsl' -or $scpPath -match '(?i)wsl') {
    throw 'The Kindle lab must use Windows OpenSSH, never WSL networking.'
}

$target = "$labUser@$labHost"
$remoteRoot = "/mnt/us/documents/kindlebrew-qa/$RunId"
$remoteScript = "$remoteRoot/tools/remote-lab.sh"
$sshOptions = @(
    '-p', [string]$labPort,
    '-o', 'ConnectTimeout=10',
    '-o', 'ServerAliveInterval=15',
    '-o', 'ServerAliveCountMax=2'
)
$scpOptions = @(
    '-P', [string]$labPort,
    '-o', 'ConnectTimeout=10',
    '-o', 'ServerAliveInterval=15',
    '-o', 'ServerAliveCountMax=2'
)

function Invoke-SshCommand {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    & $sshPath @sshOptions $target @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Windows SSH failed with exit code $LASTEXITCODE."
    }
}

function Invoke-LabCommand {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    Invoke-SshCommand -Arguments (@('sh', $remoteScript) + $Arguments)
}

function Invoke-ScpUpload {
    param(
        [Parameter(Mandatory = $true)][string[]]$Paths,
        [Parameter(Mandatory = $true)][string]$RemoteDirectory
    )
    & $scpPath @scpOptions @Paths "${target}:$RemoteDirectory/"
    if ($LASTEXITCODE -ne 0) {
        throw "Windows SCP upload failed with exit code $LASTEXITCODE."
    }
}

function Invoke-ScpDownload {
    param(
        [Parameter(Mandatory = $true)][string]$RemotePath,
        [Parameter(Mandatory = $true)][string]$LocalDirectory
    )
    & $scpPath @scpOptions '-r' "${target}:$RemotePath" $LocalDirectory
    if ($LASTEXITCODE -ne 0) {
        throw "Windows SCP download failed with exit code $LASTEXITCODE."
    }
}

switch ($Action) {
    'Connection' {
        Invoke-SshCommand -Arguments @("printf 'kindle-lab-ssh-ok\n'")
    }
    'Deploy' {
        if ([string]::IsNullOrWhiteSpace($Package)) { throw 'Deploy requires -Package.' }
        $packagePath = (Resolve-Path -LiteralPath $Package).Path
        $packageName = Split-Path -Leaf $packagePath
        if ($packageName -notmatch '^[A-Za-z0-9._-]+\.kpkg$') { throw 'Unsafe package filename.' }
        $helperNames = @('remote-lab.sh', 'close-fds-exec', 'uinput-touch', 'fbink-state', 'framebuffer-dump')
        $helperPaths = foreach ($name in $helperNames) {
            $candidate = if ($name -eq 'remote-lab.sh') {
                Join-Path $PSScriptRoot $name
            } else {
                Join-Path $buildDir $name
            }
            if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
                throw "Missing lab helper: $candidate"
            }
            (Resolve-Path -LiteralPath $candidate).Path
        }
        $packageSha = (Get-FileHash -LiteralPath $packagePath -Algorithm SHA256).Hash.ToLowerInvariant()
        $bootstrap = "umask 077; mkdir -p '$remoteRoot/tools' '$remoteRoot/stage' '$remoteRoot/run' '$remoteRoot/results' '$remoteRoot/frames' '$remoteRoot/input'"
        Invoke-SshCommand -Arguments @($bootstrap)
        Invoke-ScpUpload -Paths $helperPaths -RemoteDirectory "$remoteRoot/tools"
        Invoke-ScpUpload -Paths @($packagePath) -RemoteDirectory "$remoteRoot/stage"
        $finalize = "chmod 700 '$remoteScript' '$remoteRoot/tools/close-fds-exec' '$remoteRoot/tools/uinput-touch' '$remoteRoot/tools/fbink-state' '$remoteRoot/tools/framebuffer-dump'; printf '%s  %s\n' '$packageSha' '$packageName' > '$remoteRoot/stage/package.sha256'"
        Invoke-SshCommand -Arguments @($finalize)
        New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
        Set-Content -LiteralPath $lastRunPath -Value $RunId -Encoding ascii
        Write-Output "run_id=$RunId"
        Write-Output "remote_root=$remoteRoot"
        Write-Output "package_sha256=$packageSha"
    }
    'Inventory' { Invoke-LabCommand -Arguments @('inventory') }
    'InstallHooks' {
        if ([string]::IsNullOrWhiteSpace($Package)) { throw 'InstallHooks requires the same local -Package used by Deploy.' }
        $packagePath = (Resolve-Path -LiteralPath $Package).Path
        $packageName = Split-Path -Leaf $packagePath
        if ($packageName -notmatch '^[A-Za-z0-9._-]+\.kpkg$') { throw 'Unsafe package filename.' }
        $packageSha = (Get-FileHash -LiteralPath $packagePath -Algorithm SHA256).Hash.ToLowerInvariant()
        Invoke-LabCommand -Arguments @('install-hooks', $remoteRoot, "$remoteRoot/stage/$packageName", $packageSha)
    }
    'Run' { Invoke-LabCommand -Arguments @('run', $remoteRoot) }
    'Status' { Invoke-LabCommand -Arguments @('status', $remoteRoot) }
    'Stop' { Invoke-LabCommand -Arguments @('stop', $remoteRoot, $Signal) }
    'KillStockfish' { Invoke-LabCommand -Arguments @('kill-stockfish', $remoteRoot) }
    'Framebuffer' { Invoke-LabCommand -Arguments @('framebuffer', $remoteRoot) }
    'ProcessCheck' { Invoke-LabCommand -Arguments @('process-check') }
    'InputStart' { Invoke-LabCommand -Arguments @('input-start', $remoteRoot) }
    'Tap' { Invoke-LabCommand -Arguments @('input-send', $remoteRoot, 'tap', [string]$X, [string]$Y, [string]$DurationMs) }
    'InputDown' { Invoke-LabCommand -Arguments @('input-send', $remoteRoot, 'down', [string]$X, [string]$Y) }
    'InputMove' { Invoke-LabCommand -Arguments @('input-send', $remoteRoot, 'move', [string]$X, [string]$Y) }
    'InputUp' { Invoke-LabCommand -Arguments @('input-send', $remoteRoot, 'up') }
    'InputStop' { Invoke-LabCommand -Arguments @('input-stop', $remoteRoot) }
    'Collect' {
        Invoke-LabCommand -Arguments @('collect', $remoteRoot)
        $localRoot = Join-Path $evidenceRoot $RunId
        New-Item -ItemType Directory -Path $localRoot -Force | Out-Null
        foreach ($name in @('run', 'results', 'frames', 'input')) {
            Invoke-ScpDownload -RemotePath "$remoteRoot/$name" -LocalDirectory $localRoot
        }
        Write-Output "evidence=$localRoot"
    }
}
