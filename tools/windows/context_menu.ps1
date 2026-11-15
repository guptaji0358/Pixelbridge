<#
.SYNOPSIS
    Adds or removes the QR Studio entries in the Windows File Explorer right-click menu.

.DESCRIPTION
    Per-user only: everything is written under
        HKCU\Software\Classes\SystemFileAssociations\<.ext>\shell\QRStudio
    No administrator rights are needed and HKLM is never touched.

    The entries are ADDED next to the existing ones. Default programs, "Open with"
    lists and the menus of other file types are not modified.

      Image files (.png .jpg .jpeg .bmp .gif .webp .tif .tiff) get a "QR Studio" submenu:
          Open with QR Scanner
          Copy file path
          Open with default application
      Text files (.txt) get:
          QR Studio > Generate QR from text

    On Windows 11 these classic entries appear under "Show more options" (Shift+F10).

.PARAMETER Action
    status    (default) report what is installed; changes nothing
    install   create the keys
    uninstall remove the keys (only ones named QRStudio)

.PARAMETER ExePath
    Full path to QRStudio.exe. Required for install.

.PARAMETER DryRun
    Print every registry change without performing it.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File context_menu.ps1 -Action install -ExePath "D:\Apps\QRStudio\QRStudio.exe"
#>
[CmdletBinding()]
param(
    [ValidateSet('status', 'install', 'uninstall')]
    [string]$Action = 'status',
    [string]$ExePath,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$VerbName   = 'QRStudio'
$ClassRoot  = 'HKCU:\Software\Classes\SystemFileAssociations'
$ImageExts  = '.png', '.jpg', '.jpeg', '.bmp', '.gif', '.webp', '.tif', '.tiff'
$TextExts   = '.txt'

function Get-VerbKey([string]$ext) { Join-Path $ClassRoot "$ext\shell\$VerbName" }

function Set-Value([string]$key, [string]$name, [string]$value) {
    $shown = if ($name) { $name } else { '(Default)' }
    if ($DryRun) { Write-Output "  [dry-run] SET  $key  $shown = $value"; return }
    if (-not (Test-Path -LiteralPath $key)) { New-Item -Path $key -Force | Out-Null }
    if ($name) { New-ItemProperty -LiteralPath $key -Name $name -Value $value -PropertyType String -Force | Out-Null }
    else       { Set-Item -LiteralPath $key -Value $value }
}

function Add-SubCommand([string]$verbKey, [string]$id, [string]$label, [string]$command) {
    $sub = Join-Path $verbKey "shell\$id"
    Set-Value $sub '' $label
    Set-Value (Join-Path $sub 'command') '' $command
}

function Install-Menu {
    if (-not $ExePath) { throw 'Install needs -ExePath "<full path to QRStudio.exe>".' }
    $exe = (Resolve-Path -LiteralPath $ExePath).Path
    if ([IO.Path]::GetFileName($exe) -ne 'QRStudio.exe') { throw "Not QRStudio.exe: $exe" }
    $q = '"' + $exe + '"'

    foreach ($ext in $ImageExts) {
        $key = Get-VerbKey $ext
        Set-Value $key 'MUIVerb' 'QR Studio'
        Set-Value $key 'Icon' $exe
        Set-Value $key 'SubCommands' ''
        Add-SubCommand $key '01scan'    'Open with QR Scanner'          "$q --scan `"%1`""
        Add-SubCommand $key '02path'    'Copy file path'                "$q --copy-path `"%1`""
        Add-SubCommand $key '03default' 'Open with default application' "$q --open-default `"%1`""
    }
    foreach ($ext in $TextExts) {
        $key = Get-VerbKey $ext
        Set-Value $key 'MUIVerb' 'QR Studio'
        Set-Value $key 'Icon' $exe
        Set-Value $key 'SubCommands' ''
        Add-SubCommand $key '01generate' 'Generate QR from text' "$q --generate-from-file `"%1`""
    }
    if (-not $DryRun) { Write-Output "Installed for: $(($ImageExts + $TextExts) -join ' ')" }
}

function Uninstall-Menu {
    $removed = 0
    foreach ($ext in ($ImageExts + $TextExts)) {
        $key = Get-VerbKey $ext
        if (-not (Test-Path -LiteralPath $key)) { continue }
        if ($DryRun) { Write-Output "  [dry-run] DEL  $key" }
        else { Remove-Item -LiteralPath $key -Recurse -Force }
        $removed++

        # Tidy up parents this script may have created, only when they are empty.
        foreach ($parent in (Join-Path $ClassRoot "$ext\shell"), (Join-Path $ClassRoot $ext)) {
            if ($DryRun -or -not (Test-Path -LiteralPath $parent)) { continue }
            $item = Get-Item -LiteralPath $parent
            if ($item.SubKeyCount -eq 0 -and $item.ValueCount -eq 0) { Remove-Item -LiteralPath $parent -Force }
        }
    }
    if (-not $DryRun) { Write-Output "Removed $removed entr$(if ($removed -eq 1) {'y'} else {'ies'})." }
}

function Show-Status {
    $installed = @()
    $target = $null
    foreach ($ext in ($ImageExts + $TextExts)) {
        $key = Get-VerbKey $ext
        if (Test-Path -LiteralPath $key) {
            $installed += $ext
            if (-not $target) { $target = (Get-ItemProperty -LiteralPath $key -Name Icon -ErrorAction SilentlyContinue).Icon }
        }
    }
    if ($installed.Count -eq 0) { Write-Output 'status: not installed' }
    else {
        Write-Output "status: installed"
        Write-Output "extensions: $($installed -join ' ')"
        Write-Output "target: $target"
        if ($target -and -not (Test-Path -LiteralPath $target)) { Write-Output 'warning: target executable no longer exists' }
    }
}

switch ($Action) {
    'install'   { Install-Menu }
    'uninstall' { Uninstall-Menu }
    default     { Show-Status }
}
