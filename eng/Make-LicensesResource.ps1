$rootDir = Split-Path -Parent $PSScriptRoot

$depsPath = (Join-Path $rootDir 'src/packages/foo_scrobble-deps.*/installed/x86-windows-static-md-noiso/share/*')

$text = ''
foreach ($dep in Get-ChildItem $depsPath) {
    $copyrightFile = Join-Path $dep 'copyright'
    $text += "===== {0} {1}`n" -f $dep.Name,([String]::new('=', 40 - 7 - $dep.Name.Length))
    $text += "`n"
    $text += [IO.File]::ReadAllText($copyrightFile)
    $text += "`n`n"
}

Set-Content (Join-Path $rootDir 'src/foo_scrobble/ThirdPartyLicenses.txt') $text
