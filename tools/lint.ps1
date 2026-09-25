# lint.ps1 - the em-dash gate and friends.
#
# Fails when:
#   - any tracked text file contains U+2014 (an em dash); the repo uses "-" everywhere,
#     because the character has caused PowerShell 5.1 parse errors and log/UI mojibake;
#   - a .ps1 file starts with a UTF-8 BOM or contains a non-ASCII byte;
#   - a .ps1 file uses LF-only line endings (PowerShell 5.1 tooling expects CRLF).
#
# Pure ASCII, Windows PowerShell 5.1 compatible. Run from anywhere: it finds the repo root
# from its own location.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Push-Location $root
try {
    $files = & git ls-files
    if ($LASTEXITCODE -ne 0) { throw "git ls-files failed; run inside the repository" }

    $textExt = @('.md', '.ps1', '.txt', '.yml', '.yaml', '.json', '.ini', '.cpp', '.h', '.hpp', '.c', '.py', '.cmake', '.def', '.rc', '.xrs', '.gitignore')
    $failures = New-Object System.Collections.Generic.List[string]
    $checked = 0

    foreach ($rel in $files) {
        if (-not (Test-Path -LiteralPath $rel -PathType Leaf)) { continue }
        $ext = [System.IO.Path]::GetExtension($rel).ToLowerInvariant()
        $name = [System.IO.Path]::GetFileName($rel)
        if (($textExt -notcontains $ext) -and ($name -ne 'CMakeLists.txt') -and ($name -ne '.gitignore')) { continue }
        $checked++

        $bytes = [System.IO.File]::ReadAllBytes($rel)
        $text = [System.Text.Encoding]::UTF8.GetString($bytes)

        $line = 1
        $col = 0
        for ($i = 0; $i -lt $text.Length; $i++) {
            $ch = $text[$i]
            if ($ch -eq [char]0x2014) {
                $failures.Add("${rel}(${line}): em dash (U+2014); use '-'")
            }
            if ($ch -eq "`n") { $line++ }
        }

        if ($ext -eq '.ps1') {
            if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
                $failures.Add("${rel}(1): UTF-8 BOM in a .ps1")
            }
            for ($i = 0; $i -lt $bytes.Length; $i++) {
                if ($bytes[$i] -gt 0x7F) { $failures.Add("${rel}: non-ASCII byte at offset $i in a .ps1"); break }
            }
            if ($text.Contains("`n") -and -not $text.Contains("`r`n")) {
                $failures.Add("${rel}: LF-only line endings in a .ps1 (use CRLF)")
            }
        }
    }

    if ($failures.Count -gt 0) {
        $failures | ForEach-Object { Write-Host "LINT: $_" }
        Write-Host ("lint: FAILED, {0} finding(s) in {1} file(s) checked" -f $failures.Count, $checked)
        exit 1
    }
    Write-Host ("lint: OK, {0} file(s) checked" -f $checked)
    exit 0
}
finally {
    Pop-Location
}
